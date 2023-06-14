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

    /// The variable that holds the return value.
    /// Null when the function does not return a value.
    Var* return_val = nullptr;

    /// The final return at the end of the function block.
    /// May be null when the function returns in all blocks of a control instruction.
    Return* fn_return = nullptr;

    /// A set of control instructions that transitively hold a return instruction
    utils::Hashset<ir::ControlInstruction*, 8> holds_return_;

    /// Constructor
    /// @param mod the module
    explicit State(Module* mod) : ir(mod) {}

    /// Process the function.
    /// @param fn the function to process
    void Process(Function* fn) {
        // Find all of the nested return instructions in the function.
        utils::Vector<Return*, 4> returns;
        for (const auto& usage : fn->Usages()) {
            if (auto* ret = usage.instruction->As<Return>()) {
                TransitivelyMarkAsReturning(ret->Block()->Parent());
            }
        }

        if (holds_return_.IsEmpty()) {
            // No control instructions hold a return statement, so nothing needs to be done.
            return;
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

        // Look to see if the function ends with a return
        fn_return = tint::As<Return>(fn->StartTarget()->Branch());

        // Process the function's block.
        // This will traverse into control instructions that hold returns, and apply the necessary
        // changes to remove returns.
        ProcessBlock(fn->StartTarget());

        // If the function didn't end with a return, add one
        if (!fn_return) {
            AppendFinalReturn(fn);
        }

        // Cleanup - if 'continue_execution' was only ever assigned, remove it.
        continue_execution->DestroyIfOnlyAssigned();
    }

    /// Marks all the control instructions from ctrl to the function as holding a return.
    /// @param ctrl the control instruction to mark as returning, along with all ancestor control
    /// instructions.
    void TransitivelyMarkAsReturning(ir::ControlInstruction* ctrl) {
        for (; ctrl; ctrl = ctrl->Block()->Parent()) {
            if (!holds_return_.Add(ctrl)) {
                return;
            }
        }
    }

    /// Walks the instructions of @p block, processing control instructions that are marked as
    /// holding a return instruction. After processing a control instruction with a return, the
    /// instructions following the control instruction will be wrapped in a 'if' that only executes
    /// if a return was not reached.
    /// @param block the block to process
    void ProcessBlock(Block* block) {
        // The stack of 'if' instructions emitted after each control instruction holding a return
        // instruction. If non-empty, instructions will moved from 'block' to the true-block of the
        // 'if' at the back of this stack.
        utils::Vector<If*, 8> if_stack;

        for (auto* inst = *block->begin(); inst;) {  // For each instruction in 'block'
            // As we're modifying the block that we're iterating over, grab the pointer to the next
            // instruction before (potentially) moving 'inst' to another block.
            auto* next = inst->next;
            TINT_DEFER(inst = next);

            // Return instructions are processed *before* being moved into the 'if' block.
            if (auto* ret = inst->As<ir::Return>()) {
                ProcessReturn(ret, if_stack.IsEmpty() ? nullptr : if_stack.Back());
                break;  // All instructions processed.
            }

            // If we've already passed a control instruction holding a return, then we need to move
            // the instructions that follow the control instruction into the inner-most 'if'.
            if (!if_stack.IsEmpty()) {
                inst->Remove();
                if_stack.Back()->True()->Append(inst);
            }

            // Control instructions holding a return need to be processed, and then a new 'if' needs
            // to be created to hold the instructions that are between the control instruction and
            // the block's terminating instruction.
            if (auto* ctrl = inst->As<ir::ControlInstruction>()) {
                if (holds_return_.Contains(ctrl)) {
                    // Control instruction transitively holds a return.
                    ProcessControl(ctrl);
                    if (next && !tint::Is<Branch>(next)) {
                        if_stack.Push(CreateIfContinueExecution(ctrl));
                    }
                }
            }
        }

        SanitizeIfStack(if_stack);
    }

    /// Transforms a return instruction.
    /// @param ret the return instruction
    /// @param cond the possibly null 'if(continue_execution)' instruction for the current block.
    /// @note unlike other instructions, return instructions are not automatically moved into the
    /// 'if(continue_execution)' block.
    void ProcessReturn(Return* ret, If* cond) {
        if (ret == fn_return) {
            // 'ret' is the final instruction for the function.
            ProcessFunctionBlockReturn(ret, cond);
        } else {
            // Return is in a nested block
            ProcessNestedReturn(ret, cond);
        }
    }

    /// Transforms the return instruction that is the last instruction in the function's block.
    /// @param ret the return instruction
    /// @param cond the possibly null 'if(continue_execution)' instruction for the current block.
    void ProcessFunctionBlockReturn(Return* ret, If* cond) {
        if (!return_val) {
            return;  // No need to transform non-value, end-of-function returns
        }

        // Assign the return's value to 'return_val' inside a 'if(continue_execution)'
        if (!cond) {
            cond = CreateIfContinueExecution(ret->prev);
        }
        cond->True()->Append(b.Store(return_val, ret->Value()));
        cond->True()->Append(b.ExitIf(cond));

        // Change the function return to unconditionally load 'return_val' and return it
        auto* load = b.Load(return_val);
        load->InsertBefore(ret);
        ret->SetValue(load);
    }

    /// Transforms the return instruction that is found in a control instruction.
    /// @param ret the return instruction
    /// @param cond the possibly null 'if(continue_execution)' instruction for the current block.
    void ProcessNestedReturn(Return* ret, If* cond) {
        // If we have a 'if(continue_execution)' block, then insert instructions into that,
        // otherwise insert into the block holding the return.
        auto* block = cond ? cond->True() : ret->Block();

        // Set the 'continue_execution' flag to false, and store the return value into 'return_val',
        // if present.
        block->Append(b.Store(continue_execution, false));
        if (return_val) {
            block->Append(b.Store(return_val, ret->Args()[0]));
        }

        // If the outermost control instruction is expecting exit values, then return them as
        // 'undef' values.
        auto* ctrl = block->Parent();
        utils::Vector<ir::Value*, 8> exit_args;
        if (auto* exit_ty = tint::As<type::Tuple>(ctrl->Type())) {
            for (auto* val_ty : exit_ty->Types()) {
                exit_args.Push(b.Undef(val_ty));
            }
        }

        // Replace the return instruction with an exit instruction.
        block->Append(b.Exit(ctrl, std::move(exit_args)));
        ret->Destroy();
    }

    /// Processes all the blocks of the given control instruction.
    /// @param ctrl the control instruction
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
            },
            [&](Default) {
                diag::List diags;
                TINT_ICE(IR, diags) << "unhandled ControlInstruction: " << ctrl->TypeInfo().name;
            });
    }

    /// SanitizeIfStack ensures that all the blocks holding the 'if' instructions all end with a
    /// terminating instruction and that the innermost exit_if has its values propagated up to the
    /// outermost exit.
    /// @param if_stack the stack of 'if(continue_execution)' conditionals, with the inner most
    /// being at the end of the vector.
    void SanitizeIfStack(utils::VectorRef<If*> if_stack) {
        if (if_stack.IsEmpty()) {
            return;  // All done.
        }

        // The inner most 'if'
        auto* inner_if = if_stack.Back();

        if (inner_if->True()->HasBranchTarget()) {
            auto* exit_if = inner_if->True()->Branch()->As<ExitIf>();
            if (exit_if && !exit_if->Args().IsEmpty()) {
                // Inner-most 'if' has a 'exit_if' that returns values.
                // These need propagating through the if stack.
                auto tys = utils::Transform<8>(exit_if->Args(), [](Value* v) { return v->Type(); });
                inner_if->SetType(ir->Types().tuple(std::move(tys)));
            }
        } else {
            // Inner-most if doesn't have a terminating instruction. Add an 'exit_if'.
            inner_if->True()->Append(b.ExitIf(inner_if));
        }

        // Loop over the 'if' instructions, starting with the inner-most, and add any missing
        // terminating instructions to the blocks holding the 'if'.
        for (auto* i : utils::Reverse(if_stack)) {
            if (i->Block()->HasBranchTarget()) {
                continue;  // Block has a terminator.
            }
            utils::Vector<Value*, 8> exit_args;
            if (auto* tuple = i->Type()->As<type::Tuple>()) {
                // The 'if' has exit values. Build accessors for these so we can bubble them up.
                i->SetType(tuple);
                exit_args =
                    utils::Transform<8>(tuple->Types(), [&](const type::Type* type, size_t idx) {
                        auto* access = b.Access(type, i, static_cast<uint32_t>(idx));
                        return i->Block()->Append(access);
                    });
            }

            // Append the exit instruction to the block holding the 'if'.
            auto* exit = b.Exit(i->Block()->Parent(), std::move(exit_args));
            i->Block()->Append(exit);
        }
    }

    /// Builds instructions to create a 'if(continue_execution)' conditional.
    /// @param after new instructions will be inserted after this instruction
    /// @return the 'If' control instruction
    ir::If* CreateIfContinueExecution(ir::Instruction* after) {
        auto* load = b.Load(continue_execution);
        auto* cond = b.If(load);
        load->InsertAfter(after);
        cond->InsertAfter(load);
        return cond;
    }

    /// Adds a final return instruction to the end of @p fn
    /// @param fn the function
    void AppendFinalReturn(Function* fn) {
        auto fb = b.With(fn->StartTarget());
        if (return_val) {
            fb.Return(fn, fb.Load(return_val));
        } else {
            fb.Return(fn);
        }
    }
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
    for (auto* fn : ir->functions) {
        State state{ir};
        state.Process(fn);
    }
}

}  // namespace tint::ir::transform
