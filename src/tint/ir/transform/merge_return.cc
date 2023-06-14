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

    ir::Return* fn_ret = nullptr;

    utils::Hashset<ir::ControlInstruction*, 8> processed_;

    /// Constructor
    /// @param mod the module
    explicit State(Module* mod) : ir(mod) {}

    /// Process the function.
    /// @param func the function to process
    void Process(Function* func) {
        // Find all of the nested return instructions in the function.
        utils::Vector<Return*, 4> returns;
        for (const auto& usage : func->Usages()) {
            if (auto* ret = usage.instruction->As<Return>()) {
                if (usage.instruction->Block() == func->StartTarget()) {
                    fn_ret = ret;
                } else {
                    returns.Push(ret);
                }
            }
        }

        if (returns.IsEmpty()) {
            return;  // nothing needs to be done
        }

        // Create a boolean variable that can be used to check whether the function is returning.
        continue_execution = b.Var(ty.ptr<function, bool>());
        continue_execution->SetInitializer(b.Constant(true));
        func->StartTarget()->Prepend(continue_execution);
        ir->SetName(continue_execution, "continue_execution");

        // Create a variable to hold the return value if needed.
        if (!func->ReturnType()->Is<type::Void>()) {
            return_val = b.Var(ty.ptr(function, func->ReturnType()));
            func->StartTarget()->Prepend(return_val);
            ir->SetName(return_val, "return_value");
        }

        // Process all of the return instructions in the function.
        for (auto* ret : returns) {
            ProcessReturn(ret);
        }

        // Remove continue_execution if it was only assigned to
        DestroyIfOnlyStored(continue_execution);
    }

    void DestroyIfOnlyStored(ir::Var* var) {
        if (var->Usages().All([](const Usage& u) { return u.instruction->Is<ir::Store>(); })) {
            while (!var->Usages().IsEmpty()) {
                auto& usage = *var->Usages().begin();
                usage.instruction->Destroy();
            }
            var->Destroy();
        }
    }

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
        WrapInstructionsAfter(ctrl);

        if (ctrl->Block()) {
            // Exit from the containing block
            CreateExit(ctrl)->InsertBefore(ret);

            // Remove the old return
            ret->Destroy();
        }
    }

    ir::Branch* CreateExit(ir::ControlInstruction* ctrl) {
        if (auto* exit_ty = ctrl->Type()) {
            return tint::Switch(ctrl,  //
                                [&](ir::If* i) { return b.ExitIf(i, b.Undef(exit_ty)); });
        } else {
            return tint::Switch(ctrl,  //
                                [&](ir::If* i) { return b.ExitIf(i); });
        }
    }

    void WrapInstructionsAfter(ir::ControlInstruction* ctrl) {
        if (!processed_.Add(ctrl)) {
            return;  // Already processed this instruction.
        }
        if (!ctrl->next) {
            return;  // No instructions after ctrl to wrap.
        }
        auto* load = b.Load(continue_execution);
        auto* cond = b.If(load);
        while (auto* inst = ctrl->next) {
            if (inst == fn_ret) {  // inst is the last return of the function.
                if (return_val) {  // The return has a value.
                    // Store the return value to 'return_val' at the end of 'cond'
                    cond->True()->Append(b.Store(return_val, fn_ret->Value()));
                    // And change the return to unconditionally load 'return_val' and return it
                    auto* val_ret = b.Load(return_val);
                    val_ret->InsertBefore(fn_ret);
                    fn_ret->SetValue(val_ret);
                }
                break;
            }

            // Move the instruction out of 'block' and into 'cond->True()'
            inst->Remove();
            cond->True()->Append(inst);
        }
        if (!cond->True()->IsEmpty()) {
            cond->True()->Append(b.ExitIf(cond));
            load->InsertAfter(ctrl);
            cond->InsertAfter(load);
        } else {
            cond->Destroy();
            load->Destroy();
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
    for (auto* func : ir->functions) {
        State state(ir);
        state.Process(func);
    }
}

}  // namespace tint::ir::transform
