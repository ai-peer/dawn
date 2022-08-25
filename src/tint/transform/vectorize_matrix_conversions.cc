// Copyright 2021 The Tint Authors.
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

#include "src/tint/transform/vectorize_matrix_conversions.h"

#include <unordered_map>
#include <utility>

#include "src/tint/program_builder.h"
#include "src/tint/sem/abstract_numeric.h"
#include "src/tint/sem/call.h"
#include "src/tint/sem/expression.h"
#include "src/tint/sem/type_conversion.h"
#include "src/tint/utils/map.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::VectorizeMatrixConversions);

namespace tint::transform {

VectorizeMatrixConversions::VectorizeMatrixConversions() = default;

VectorizeMatrixConversions::~VectorizeMatrixConversions() = default;

bool VectorizeMatrixConversions::ShouldRun(const Program* program, const DataMap&) const {
    for (auto* node : program->ASTNodes().Objects()) {
        if (auto* call = program->Sem().Get<sem::Call>(node)) {
            if (call->Target()->Is<sem::TypeConversion>() && call->Type()->Is<sem::Matrix>()) {
                auto& args = call->Arguments();
                if (args.Length() == 1 && args[0]->Type()->UnwrapRef()->is_float_matrix()) {
                    return true;
                }
            }
        }
    }
    return false;
}

void VectorizeMatrixConversions::Run(CloneContext& ctx, const DataMap&, DataMap&) const {
    std::unordered_map<const sem::Matrix*, Symbol> scalar_ctors;

    ctx.ReplaceAll([&](const ast::CallExpression* expr) -> const ast::CallExpression* {
        auto* call = ctx.src->Sem().Get(expr)->UnwrapMaterialize()->As<sem::Call>();
        auto* ty_conv = call->Target()->As<sem::TypeConversion>();
        if (!ty_conv) {
            return nullptr;
        }
        auto* dst_type = call->Type()->As<sem::Matrix>();
        if (!dst_type) {
            return nullptr;
        }

        auto& args = call->Arguments();
        if (args.Length() != 1) {
            return nullptr;
        }

        auto& src = args[0];

        auto* src_type = args[0]->Type()->UnwrapRef()->As<sem::Matrix>();
        if (!src_type) {
            return nullptr;
        }

        // The source and destination type of a matrix conversion must have a same shape.
        TINT_ASSERT(Transform, src_type->rows() == dst_type->rows() &&
                                   src_type->columns() == dst_type->columns());

        utils::Vector<const ast::Expression*, 4> columns;
        for (uint32_t c = 0; c < dst_type->columns(); c++) {
            columns.Push(ctx.dst->Construct(CreateASTTypeFor(ctx, dst_type->ColumnType()),
                                            ctx.dst->IndexAccessor(ctx.Clone(src->Declaration()),
                                                                   ctx.dst->Expr(tint::AInt(c)))));
        }
        return ctx.dst->Construct(CreateASTTypeFor(ctx, dst_type), columns);

        /*
        // Constructs a matrix using vector columns, with the elements constructed using the
        // 'element(uint32_t c, uint32_t r)' callback.
        auto build_mat = [&](auto&& element) {
            utils::Vector<const ast::Expression*, 4> columns;
            for (uint32_t c = 0; c < dst_type->columns(); c++) {
                utils::Vector<const ast::Expression*, 4> row_values;
                for (uint32_t r = 0; r < dst_type->rows(); r++) {
                    row_values.Push(element(c, r));
                }

                // Construct the column vector.
                columns.Push(ctx.dst->vec(CreateASTTypeFor(ctx, dst_type->type()), dst_type->rows(),
                                          std::move(row_values)));
            }
            return ctx.dst->Construct(CreateASTTypeFor(ctx, dst_type), columns);
        };

        if (args.Length() == 1) {
            // Generate a helper function for constructing the matrix.
            // This is done to ensure that the single argument value is only evaluated once, and
            // with the correct expression evaluation order.
            auto fn = utils::GetOrCreate(scalar_ctors, dst_type, [&] {
                auto name =
                    ctx.dst->Symbols().New("build_mat" + std::to_string(dst_type->columns()) + "x" +
                                           std::to_string(dst_type->rows()));
                ctx.dst->Func(name,
                              utils::Vector{
                                  // Single scalar parameter
                                  ctx.dst->Param("value", CreateASTTypeFor(ctx, dst_type->type())),
                              },
                              CreateASTTypeFor(ctx, dst_type),
                              utils::Vector{
                                  ctx.dst->Return(build_mat([&](uint32_t, uint32_t) {  //
                                      return ctx.dst->Expr("value");
                                  })),
                              });
                return name;
            });
            return ctx.dst->Call(fn, ctx.Clone(args[0]->Declaration()));
        }

        if (args.Length() == dst_type->columns() * dst_type->rows()) {
            return build_mat([&](uint32_t c, uint32_t r) {
                return ctx.Clone(args[c * dst_type->rows() + r]->Declaration());
            });
        }

        TINT_ICE(Transform, ctx.dst->Diagnostics())
            << "matrix constructor has unexpected number of arguments";
        return nullptr;
        //*/
    });

    ctx.Clone();
}

}  // namespace tint::transform
