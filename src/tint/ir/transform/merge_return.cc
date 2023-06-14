// Copyright 2023 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/tint/ir/transform/merge_return.h"

#include <utility>

#include "src/tint/ir/builder.h"
#include "src/tint/ir/module.h"
#include "src/tint/switch.h"
#include "src/tint/utils/reverse.h"
#include "src/tint/utils/transform.h"

TINT_INSTANTIATE_TYPEINFO(tint::ir::transform::MergeReturn);

using namespace tint::builtin::fluent_types;  // NOLINT
using namespace tint::number_suffixes;        // NOLINT

namespace tint::ir::transform {

MergeReturn::MergeReturn() = default;

MergeReturn::~MergeReturn() = default;

/// PIMPL state for the transform, for a single function.
struct MergeReturn::State {
    /// The IR module.
    Module* ir = nullptr;
    /// The IR builder.
    Builder b{*ir};
    /// The type manager.
    type::Manager& ty{ir->Types()};

    /// The "has not returned" flag.
    Var* continue_execution = nullptr;

    /// The return value.
    Var* return_val = nullptr;

    Function* fn = nullptr;

    Return* fn_return = nullptr;

    utils::Hashset<ir::ControlInstruction*, 8> returns_;

    /// Constructor
    /// @param mod the module
    /// @param func the function to process
    explicit State(Module* mod, Function* func) : ir(mod), fn(func) {}

    /// Process the function.
    void Process() {
        // Find all of the nested return instructions in the function.
        utils::Vector<Return*, 4> returns;
        for (const auto& usage : fn->Usages()) {
            if (auto* ret = usage.instruction->As<Return>()) {
                TransitivelyMarkAsReturning(ret->Block()->Parent());
            }
        }

        if (returns_.IsEmpty()) {
            return;  // nothing needs to be done
        }

        // Create a boolean variable that can be used to check whether the function is returning.
        continue_execution = b.Var(ty.ptr<function, bool>());
        continue_execution->SetInitializer(b.Constant(true));
        fn->StartTarget()->Prepend(continue_execution);
        ir->SetName(continue_execution, "continue_execution");

        // Create a variable to hold the return value if needed.
        if (!fn->ReturnType()->Is<type::Void>()) {
            return_val = b.Var(ty.ptr(function, fn->ReturnType()));
            fn->StartTarget()->Prepend(return_val);
            ir->SetName(return_val, "return_value");
        }

        fn_return = tint::As<Return>(fn->StartTarget()->Back());

        ProcessBlock(fn->StartTarget());

        if (!fn->StartTarget()->HasBranchTarget()) {
            // Function does not end with a return. Add one.
            auto fb = b.With(fn->StartTarget());
            if (return_val) {
                fb.Return(fn, fb.Load(return_val));
            } else {
                fb.Return(fn);
            }
        }

        DestroyIfOnlyAssigned(continue_execution);
    }

    void TransitivelyMarkAsReturning(ir::ControlInstruction* ctrl) {
        for (; ctrl; ctrl = ctrl->Block()->Parent()) {
            if (!returns_.Add(ctrl)) {
                return;
            }
        }
    }

    void ProcessBlock(Block* block) {
        auto* inst = *block->begin();

        utils::Vector<If*, 8> if_stack;
        while (inst) {
            auto* next = inst->next;
            TINT_DEFER(inst = next);

            if (auto* ret = inst->As<ir::Return>()) {
                ProcessReturn(ret, if_stack.IsEmpty() ? nullptr : if_stack.Back());
                break;
            }

            if (!if_stack.IsEmpty()) {
                // Move the instruction from block to cond->True()
                inst->Remove();
                if_stack.Back()->True()->Append(inst);
            }

            if (auto* ctrl = inst->As<ir::ControlInstruction>()) {
                if (returns_.Contains(ctrl)) {
                    ProcessControl(ctrl);
                    if (next && !tint::Is<Branch>(next)) {
                        if_stack.Push(CreateIfContinueExecution(ctrl));
                    }
                }
            }
        }

        if (!if_stack.IsEmpty()) {
            // Ensure that the chain of conditions all end with a branch target, and that exit
            // values are propagated.
            auto* inner_if = if_stack.Back();
            if (auto* exit = tint::As<ExitIf>(inner_if->True()->Branch());
                exit && !exit->Args().IsEmpty()) {
                auto tys = utils::Transform<8>(exit->Args(), [](Value* v) { return v->Type(); });
                inner_if->SetType(ir->Types().tuple(std::move(tys)));
            } else if (!inner_if->True()->HasBranchTarget()) {
                inner_if->True()->Append(b.ExitIf(inner_if));
            }
            for (auto* i : utils::Reverse(if_stack)) {
                if (!i->Block()->HasBranchTarget()) {
                    utils::Vector<Value*, 8> exit_args;
                    if (auto* tuple = i->Type()->As<type::Tuple>()) {
                        i->SetType(tuple);
                        exit_args = utils::Transform<8>(
                            tuple->Types(), [&](const type::Type* type, size_t idx) {
                                auto* access = b.Access(type, i, static_cast<uint32_t>(idx));
                                return i->Block()->Append(access);
                            });
                    }
                    i->Block()->Append(CreateExit(i->Block()->Parent(), std::move(exit_args)));
                }
            }
        }
    }

    void ProcessReturn(Return* ret, If* cond) {
        if (ret == fn_return) {
            // Return instruction for the function.
            if (return_val) {
                // The return has a value.
                // Conditionally store the return's value to 'return_val'
                if (!cond) {
                    cond = CreateIfContinueExecution(ret->prev);
                }
                cond->True()->Append(b.Store(return_val, ret->Value()));
                cond->True()->Append(b.ExitIf(cond));

                // Change the return to unconditionally load 'return_val' and return it
                auto* load = b.Load(return_val);
                load->InsertBefore(ret);
                ret->SetValue(load);
            }
        } else {
            // Return is in a nested block
            auto* block = cond ? cond->True() : ret->Block();

            // Set the 'continue_execution' flag to false, and record the return value if present.
            block->Append(b.Store(continue_execution, false));
            if (return_val) {
                block->Append(b.Store(return_val, ret->Args()[0]));
            }

            auto* ctrl = block->Parent();
            utils::Vector<ir::Value*, 8> exit_args;
            if (auto* exit_ty = tint::As<type::Tuple>(ctrl->Type())) {
                for (auto* val_ty : exit_ty->Types()) {
                    exit_args.Push(b.Undef(val_ty));
                }
            }

            block->Append(CreateExit(ctrl, std::move(exit_args)));
            ret->Destroy();
        }
    }

    void ProcessControl(ir::ControlInstruction* ctrl) {
        tint::Switch(
            ctrl,  //
            [&](ir::If* i) {
                ProcessBlock(i->True());
                ProcessBlock(i->False());
            },
            [&](ir::Loop* i) {
                ProcessBlock(i->Initializer());
                ProcessBlock(i->Body());
                ProcessBlock(i->Continuing());
            },
            [&](ir::Switch* i) {
                for (auto& c : i->Cases()) {
                    ProcessBlock(c.Block());
                }
            });
    }

    ir::If* CreateIfContinueExecution(ir::Instruction* after) {
        auto* load = b.Load(continue_execution);
        auto* cond = b.If(load);
        load->InsertAfter(after);
        cond->InsertAfter(load);
        return cond;
    }

    ir::Branch* CreateExit(ir::ControlInstruction* target,
                           utils::VectorRef<ir::Value*> args = utils::Empty) {
        return tint::Switch(
            target,  //
            [&](ir::If* i) { return b.ExitIf(i, args); },
            [&](ir::Loop* i) { return b.ExitLoop(i, args); },
            [&](ir::Switch* i) { return b.ExitSwitch(i, args); });
    }

    void DestroyIfOnlyAssigned(ir::Var* var) {
        if (var->Usages().All([](const Usage& u) { return u.instruction->Is<ir::Store>(); })) {
            while (!var->Usages().IsEmpty()) {
                auto& usage = *var->Usages().begin();
                usage.instruction->Destroy();
            }
            var->Destroy();
        }
    }

#if 0
    /// Process a return instruction.
    /// @param ret the return instruction
    void ProcessReturn(Return* ret) {
        // Set the 'continue_execution' flag to false, and record the return value if present.
        b.Store(continue_execution, b.Constant(false))->InsertBefore(ret);
        if (return_val) {
            b.Store(return_val, ret->Args()[0])->InsertBefore(ret);
        }

        // Find the outer control instruction
        auto* ctrl = ret->Block()->Parent();

        // Process the instructions that follow ctrl
        RecursivelyWrapInstructionsAfter(ctrl);

        if (ctrl->Block()) {
            // Exit from the block holding the return
            utils::Vector<ir::Value*, 8> exit_args;
            if (auto* exit_ty = tint::As<type::Tuple>(ctrl->Type())) {
                for (auto* val_ty : exit_ty->Types()) {
                    exit_args.Push(b.Undef(val_ty));
                }
            }
            CreateExit(ctrl, std::move(exit_args))->InsertBefore(ret);

            // Remove the old return
            ret->Destroy();
        }
    }

    ir::Branch* CreateExit(ir::ControlInstruction* target,
                           utils::VectorRef<ir::Value*> args = utils::Empty) {
        return tint::Switch(target,  //
                            [&](ir::If* i) { return b.ExitIf(i, args); });
    }

    void RecursivelyWrapInstructionsAfter(ir::ControlInstruction* ctrl) {
        for (; ctrl; ctrl = ctrl->Block()->Parent()) {
            if (!processed_.Add(ctrl)) {
                return;  // Already processed this instruction.
            }
            WrapInstructionsAfter(ctrl);
        }
    }

    void WrapInstructionsAfter(ir::ControlInstruction* ctrl) {
        ir::If* cond = nullptr;
        auto make_cond = [&] {
            if (!cond) {
                auto* load = b.Load(continue_execution);
                cond = b.If(load);
                load->InsertAfter(ctrl);
                cond->InsertAfter(load);
            }
        };

        ir::Branch* exit = ctrl->next->As<Branch>();
        if (!exit) {
            make_cond();
            while (auto* inst = cond->next) {
                if (auto* e = inst->As<Branch>()) {
                    exit = e;
                    break;
                }
                // Move the instruction out of the 'ctrl->Block()' and into 'cond->True()'
                inst->Remove();
                cond->True()->Append(inst);
            }
        }

        if (exit == fn_ret) {
            // exit is the last return of the function
            if (return_val) {
                // The return has a value.
                // Store the return value to 'return_val' at the end of 'cond'
                make_cond();
                cond->True()->Append(b.Store(return_val, fn_ret->Value()));
                cond->True()->Append(b.ExitIf(cond));
                // And change the return to unconditionally load 'return_val' and return it
                auto* val_ret = b.Load(return_val);
                val_ret->InsertBefore(fn_ret);
                fn_ret->SetValue(val_ret);
                return;
            }

            if (cond) {
                cond->True()->Append(b.ExitIf(cond));
            }
            return;
        }

        // Last instruction in a nested block
        if (!cond) {
            return;  // No instructions between ctrl and the branch. Nothing to do.
        }

        utils::Vector<ir::Value*, 4> inner_args;
        utils::Vector<ir::Value*, 4> outer_args;
        if (auto* exit_ty = tint::As<type::Tuple>(exit->Block()->Parent()->Type())) {
            cond->SetType(exit_ty);
            for (auto* arg : exit->Args()) {
                auto* access = b.Access(arg->Type(), cond, i32(outer_args.Length()));
                access->InsertBefore(exit);
                outer_args.Push(access);
                inner_args.Push(arg);
            }
        }

        cond->True()->Append(b.ExitIf(cond, std::move(inner_args)));
        exit->SetOperands(std::move(outer_args));
    }
#endif
};

void MergeReturn::Run(Module* ir, const DataMap&, DataMap&) const {
    Builder builder(*ir);

    // Find all return instructions.
    utils::Hashmap<Function*, utils::Vector<Return*, 4>, 4> function_to_returns;
    for (auto* inst : ir->values.Objects()) {
        if (auto* ret = inst->As<Return>(); ret && ret->Block()) {
            auto& returns = function_to_returns.GetOrCreate(
                ret->Func(), [&]() { return utils::VectorRef<Return*>{}; });
            returns.Push(ret);
        }
    }

    // Process each function.
    for (auto* func : ir->functions) {
        State state{ir, func};
        state.Process();
    }
}

}  // namespace tint::ir::transform
