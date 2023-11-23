// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/tint/lang/spirv/writer/raise/negate_branch_conditions.h"

#include <utility>

#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/core/ir/validator.h"
#include "src/tint/lang/spirv/ir/builtin_call.h"

using namespace tint::core::number_suffixes;  // NOLINT
using namespace tint::core::fluent_types;     // NOLINT

namespace tint::spirv::writer::raise {

namespace {

/// PIMPL state for the transform.
struct State {
    /// The IR module.
    core::ir::Module& ir;

    /// The IR builder.
    core::ir::Builder b{ir};

    /// Process the module.
    void Process() {
        // Find if instructions and negate their conditions.
        for (auto* inst : ir.instructions.Objects()) {
            auto* if_ = inst->As<core::ir::If>();
            if (if_ && if_->Alive()) {
                NegateCondition(if_);
            }
        }
    }

    /// Insert OpLogicalNot instructions before the condition of each if instruction.
    /// @param if_ the if instruction to modify
    void NegateCondition(core::ir::If* if_) {
        // make_not() creates an OpLogicalNot instruction for a given value and returns the result.
        auto make_not = [&](core::ir::Value* value) {
            return b
                .Call<spirv::ir::BuiltinCall>(value->Type(), spirv::BuiltinFn::kLogicalNot, value)
                ->Result();
        };

        // maybe_invert() tries to invert a condition by inverting it's integer comparison opcode,
        // and returns the logical negation of the inverted result.
        auto maybe_invert = [&](core::ir::Value* cond) -> core::ir::Value* {
            // Do not try to invert conditions that are used in more than one place.
            if (cond->Usages().Count() != 1u) {
                return nullptr;
            }

            // Check if the condition is the result of an integer binary instruction.
            auto* result = cond->As<core::ir::InstructionResult>();
            if (!result) {
                return nullptr;
            }
            auto* binary = result->Instruction()->As<core::ir::Binary>();
            if (!binary || !binary->LHS()->Type()->is_integer_scalar()) {
                return nullptr;
            }

            // Invert the comparison.
            core::ir::Binary* inverted = nullptr;
            switch (binary->Op()) {
                case core::ir::BinaryOp::kEqual:
                    inverted = b.NotEqual(cond->Type(), binary->LHS(), binary->RHS());
                    break;
                case core::ir::BinaryOp::kGreaterThan:
                    inverted = b.LessThanEqual(cond->Type(), binary->LHS(), binary->RHS());
                    break;
                case core::ir::BinaryOp::kGreaterThanEqual:
                    inverted = b.LessThan(cond->Type(), binary->LHS(), binary->RHS());
                    break;
                case core::ir::BinaryOp::kLessThan:
                    inverted = b.GreaterThanEqual(cond->Type(), binary->LHS(), binary->RHS());
                    break;
                case core::ir::BinaryOp::kLessThanEqual:
                    inverted = b.GreaterThan(cond->Type(), binary->LHS(), binary->RHS());
                    break;
                case core::ir::BinaryOp::kNotEqual:
                    inverted = b.Equal(cond->Type(), binary->LHS(), binary->RHS());
                    break;
                default:
                    return nullptr;
            }

            // Destroy the original comparison.
            binary->Destroy();

            // Negate the inverted comparison result.
            return make_not(inverted->Result());
        };

        b.InsertBefore(if_, [&] {
            auto* cond = if_->Condition();

            // Try to invert the comparison used for the condition.
            if (auto* inverted = maybe_invert(cond)) {
                if_->SetOperand(core::ir::If::kConditionOperandOffset, inverted);
                return;
            }

            // Insert two logical negations (which cancel each other out).
            auto* not_1 = make_not(cond);
            auto* not_2 = make_not(not_1);
            if_->SetOperand(core::ir::If::kConditionOperandOffset, not_2);
        });
    }
};

}  // namespace

Result<SuccessType> NegateBranchConditions(core::ir::Module& ir) {
    auto result = ValidateAndDumpIfNeeded(ir, "NegateBranchConditions transform");
    if (!result) {
        return result;
    }

    State{ir}.Process();

    return Success;
}

}  // namespace tint::spirv::writer::raise
