// Copyright 2024 The Dawn & Tint Authors
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

#include "src/tint/lang/wgsl/ast/transform/fold_constants.h"

#include "src/tint/lang/core/constant/composite.h"
#include "src/tint/lang/core/constant/scalar.h"
#include "src/tint/lang/core/constant/splat.h"
#include "src/tint/lang/core/fluent_types.h"
#include "src/tint/lang/wgsl/program/clone_context.h"
#include "src/tint/lang/wgsl/program/program_builder.h"
#include "src/tint/lang/wgsl/resolver/resolve.h"
#include "src/tint/lang/wgsl/sem/index_accessor_expression.h"
#include "src/tint/lang/wgsl/sem/variable.h"
#include "src/tint/utils/rtti/switch.h"

using namespace tint::core::fluent_types;  // NOLINT

TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::FoldConstants);

namespace tint::ast::transform {
namespace {

bool expand(const core::constant::Value* value) {
    return tint::Switch(  //
        value,            //
        [&](const core::constant::ScalarBase*) { return true; },
        [&](const core::constant::Splat* splat) { return expand(splat->Index(0)); },
        [&](const core::constant::Composite* comp) {
            return Switch(
                comp->Type(),  //
                [&](const core::type::Vector*) { return true; },
                [&](const core::type::Matrix*) { return true; },
                [&](const core::type::Struct*) { return false; },
                [&](const core::type::Array*) { return true; },  //
                TINT_ICE_ON_NO_MATCH);
        });
}

const ast::Expression* expr_for_value(ProgramBuilder* b,
                                      program::CloneContext* ctx,
                                      const core::constant::Value* value) {
    return tint::Switch(  //
        value,            //
        [&](const core::constant::ScalarBase* sb) {
            return tint::Switch(  //
                sb,               //
                [&](const core::constant::Scalar<bool>* bool_) {
                    return b->Expr(bool_->ValueOf());
                },
                [&](const core::constant::Scalar<f16>* f16) {
                    return b->Expr(core::f16(f16->ValueOf()));
                },
                [&](const core::constant::Scalar<f32>* f32) {
                    return b->Expr(core::f32(f32->ValueOf()));
                },
                [&](const core::constant::Scalar<i32>* i32) {
                    return b->Expr(core::i32(i32->ValueOf()));
                },
                [&](const core::constant::Scalar<u32>* u32) {
                    return b->Expr(core::u32(u32->ValueOf()));
                },
                TINT_ICE_ON_NO_MATCH);
        },
        [&](const core::constant::Splat* splat) {
            return b->Call(FoldConstants::CreateASTTypeFor(*ctx, splat->Type()),
                           expr_for_value(b, ctx, splat->Index(0)));
        },
        [&](const core::constant::Composite* comp) {
            return Switch(
                comp->Type(),  //
                [&](const core::type::Vector* vec) {
                    return b->Call(FoldConstants::CreateASTTypeFor(
                        *ctx, vec->type()));  //, const core::type::Type *ty))
                },
                [&](const core::type::Matrix* mat) {
                    ctx.Replace(expr, [&]() -> const ast::Expression* {
                        return b.Call(mat, comp->elements);
                    });
                },

                [&](const core::type::Array* arr) {
                    ctx.Replace(expr, [&]() -> const ast::Expression* {
                        return b.Call(arr, comp->elements);
                    });
                },
                TINT_ICE_ON_NO_MATCH);
        }, );
}

}  // namespace

FoldConstants::FoldConstants() = default;

FoldConstants::~FoldConstants() = default;

Transform::ApplyResult FoldConstants::Apply(const Program& src, const DataMap&, DataMap&) const {
    ProgramBuilder b;
    program::CloneContext ctx{&b, &src, /* auto_clone_symbols */ true};

    for (auto* node : src.ASTNodes().Objects()) {
        auto* expr = node->As<ast::Expression>();
        if (!expr) {
            continue;
        }

        auto& sem = ctx.src->Sem();
        auto* ve = sem.Get<sem::ValueExpression>(expr);
        // No sem node found
        if (!ve) {
            continue;
        }

        auto* cv = ve->ConstantValue();
        // No constant value for this expression
        if (!cv) {
            continue;
        }

        if (expand(cv)) {
            ctx.Replace(expr, [&]() { return expr_for_value(&b, &ctx, cv); });
        }
    }

    ctx.Clone();
    return resolver::Resolve(b);
}

}  // namespace tint::ast::transform
