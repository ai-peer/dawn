// Copyright 2022 The Tint Authors.
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

#include "src/tint/transform/std140.h"

#include <algorithm>
#include <utility>
#include <variant>

#include "src/tint/program_builder.h"
#include "src/tint/sem/index_accessor_expression.h"
#include "src/tint/sem/member_accessor_expression.h"
#include "src/tint/sem/struct.h"
#include "src/tint/sem/variable.h"
#include "src/tint/utils/hashmap.h"
#include "src/tint/utils/transform.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::Std140);

using namespace tint::number_suffixes;  // NOLINT

namespace {

struct DynamicIndex {
    size_t slot;  // The i'th dynamic index for the access chain
};

bool operator!=(const DynamicIndex& a, const DynamicIndex& b) {
    return a.slot == b.slot;
}

}  // namespace

namespace tint::utils {

/// Hasher specialization for DynamicIndex
template <>
struct Hasher<DynamicIndex> {
    uint64_t operator()(const DynamicIndex& d) const { return utils::Hash(d.slot); }
};

}  // namespace tint::utils

namespace tint::transform {

namespace {

using AccessIndex = std::variant<u32, DynamicIndex>;

using AccessChain = utils::Vector<AccessIndex, 8>;

struct MatrixAccess {
    const sem::GlobalVariable* var;
    AccessChain chain;
    utils::Vector<const sem::Expression*, 8> dynamic_indices;
    const sem::Matrix* matrix_type = nullptr;  // The type of the matrix being indexed
    size_t matrix_chain_idx = ~0u;             // The index of the matrix access in chain
};

enum LoadStore { kLoad, kStore };

struct LoadStoreFn {
    const sem::GlobalVariable* var;
    AccessChain access_chain;
    LoadStore op;

    /// Hash function for LoadFn.
    struct Hasher {
        uint64_t operator()(const LoadStoreFn& fn) const {
            return utils::Hash(fn.var, fn.access_chain, fn.op);
        }
    };
};

bool operator==(const LoadStoreFn& a, const LoadStoreFn& b) {
    return a.var == b.var && a.access_chain == b.access_chain && a.op == b.op;
}

bool MatrixNeedsDecomposing(const sem::Matrix* mat) {
    return mat->ColumnStride() == 8;
}

enum class Action { kStop, kContinue };
template <typename F>
void ForeachMemberNeedsDecomposing(const Program* program, F&& cb) {
    for (auto* ty : program->Types()) {
        if (auto* str = ty->As<sem::Struct>()) {
            if (str->UsedAs(ast::StorageClass::kUniform)) {
                for (auto* member : str->Members()) {
                    if (auto* mat = member->Type()->As<sem::Matrix>()) {
                        if (MatrixNeedsDecomposing(mat)) {
                            if (cb(member) == Action::kContinue) {
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
}

std::string UniqueName(const ast::Struct* str,
                       Symbol unsuffixed,
                       const SymbolTable& symbols,
                       uint32_t count) {
    auto prefix = symbols.NameFor(unsuffixed);
    while (true) {
        prefix += "_";

        utils::Hashset<std::string, 4> strings;
        for (uint32_t i = 0; i < count; i++) {
            strings.Add(prefix + std::to_string(i));
        }

        bool unique = true;
        for (auto* member : str->members) {
            if (strings.Contains(symbols.NameFor(member->symbol))) {
                unique = false;
                break;
            }
        }

        if (unique) {
            return prefix;
        }
    }
}

}  // namespace

Std140::Std140() = default;

Std140::~Std140() = default;

bool Std140::ShouldRun(const Program* program, const DataMap&) const {
    bool matrix_needs_decomposing = false;
    ForeachMemberNeedsDecomposing(program, [&](...) {
        matrix_needs_decomposing = true;
        return Action::kStop;
    });
    return matrix_needs_decomposing;
}

void Std140::Run(CloneContext& ctx, const DataMap&, DataMap&) const {
    const auto& sem = ctx.src->Sem();
    const auto& sym = ctx.src->Symbols();
    auto& b = *ctx.dst;

    struct {
        // Map of structure member in ctx.src of a matrix type, to list of decomposed column member
        // symbols in ctx.dst.
        utils::Hashmap<const sem::StructMember*, utils::Vector<Symbol, 4>, 8> mat;

        /// Map of load / store function signature, to the generated function
        utils::Hashmap<LoadStoreFn, Symbol, 8, LoadStoreFn::Hasher> load_store_fns;
    } decomposed;

    // Split the matrices that need decomposing
    ForeachMemberNeedsDecomposing(ctx.src, [&](const sem::StructMember* member) {
        // Structure member of matrix type needs decomposition.
        // Replace the member with a column vectors.
        const auto* str = member->Struct()->Declaration();
        const auto* mat = member->Type()->As<sem::Matrix>();
        const auto num_columns = mat->columns();
        const auto name = UniqueName(str, member->Name(), sym, num_columns);
        utils::Vector<Symbol, 4> column_sym;

        for (uint32_t i = 0; i < num_columns; i++) {
            utils::Vector<const ast::Attribute*, 1> attributes;
            if ((i == num_columns - 1) && mat->Size() != member->Size()) {
                // The matrix was @size() annotated with a larger size than the natural size for the
                // matrix. This extra padding needs to be applied to the last column vector.
                attributes.Push(
                    b.MemberSize(member->Size() - mat->ColumnType()->Size() * (num_columns - 1)));
            }

            const auto col_name = name + std::to_string(i);
            const auto col_sym = ctx.dst->Symbols().New(col_name);
            const auto* col_ty = CreateASTTypeFor(ctx, mat->ColumnType());
            const auto* col_mem = ctx.dst->Member(col_sym, col_ty, std::move(attributes));
            // Add the column vector to the structure
            ctx.InsertBefore(str->members, member->Declaration(), col_mem);
            // Record the column vector member names
            column_sym.Push(col_sym);
        }
        // Remove the now-decomposed matrix from the structure
        ctx.Remove(str->members, member->Declaration());
        // Record the matrix decomposition.
        decomposed.mat.Add(member, std::move(column_sym));
        return Action::kContinue;
    });

    auto get_access = [&](const sem::Expression* expr) -> std::optional<MatrixAccess> {
        MatrixAccess access;

        access.var = tint::As<sem::GlobalVariable>(expr->SourceVariable());
        if (!access.var) {
            // Early out for expressions that aren't from a global
            return std::nullopt;
        }

        while (true) {
            auto action = Switch(
                expr,  //
                [&](const sem::VariableUser*) { return Action::kStop; },
                [&](const sem::StructMemberAccess* a) {
                    if (!access.matrix_type && decomposed.mat.Contains(a->Member())) {
                        access.matrix_chain_idx = access.chain.Length();
                        access.matrix_type = expr->Type()->As<sem::Matrix>();
                    }
                    access.chain.Push(u32(a->Member()->Index()));
                    expr = a->Object();
                    return Action::kContinue;
                },
                [&](const sem::IndexAccessorExpression* a) {
                    if (auto* val = a->Index()->ConstantValue()) {
                        access.chain.Push(val->As<u32>());
                    } else {
                        access.chain.Push(DynamicIndex{});
                        access.dynamic_indices.Push(a->Index());
                    }
                    expr = a->Object();
                    return Action::kContinue;
                },
                [&](const sem::Swizzle* s) {
                    if (s->Indices().Length() == 1) {
                        access.chain.Push(u32(s->Indices()[0]));
                    } else {
                        TINT_UNIMPLEMENTED(Transform, b.Diagnostics())
                            << "Swizzles are not implemented yet";
                        return Action::kStop;
                    }
                    expr = s->Object();
                    return Action::kContinue;
                },
                [&](Default) { return Action::kStop; });
            if (action == Action::kStop) {
                break;
            }
        }

        if (!access.matrix_type) {
            // Expression chain didn't use any decomposed matrices.
            return std::nullopt;
        }

        std::reverse(access.chain.begin(), access.chain.end());
        std::reverse(access.dynamic_indices.begin(), access.dynamic_indices.end());

        return access;
    };

    struct ExprTypeName {
        const ast::Expression* expr;
        const sem::Type* type;
        std::string name;
    };

    auto build_access_expr =
        [&](const ast::Expression* lhs, const sem::Type* ty, AccessIndex access,
            utils::VectorRef<const ast::Expression*> dynamic_indices) -> ExprTypeName {
        if (auto* dyn_idx = std::get_if<DynamicIndex>(&access)) {
            auto name = "p" + std::to_string(dyn_idx->slot);
            return Switch(
                ty,  //
                [&](const sem::Array* arr) -> ExprTypeName {
                    auto* param = dynamic_indices[dyn_idx->slot];
                    auto* expr = b.IndexAccessor(lhs, param);
                    return {expr, arr->ElemType(), name};
                },  //
                [&](const sem::Matrix* mat) -> ExprTypeName {
                    auto* param = dynamic_indices[dyn_idx->slot];
                    auto* expr = b.IndexAccessor(lhs, param);
                    return {expr, mat->ColumnType(), name};
                },  //
                [&](const sem::Vector* vec) -> ExprTypeName {
                    auto* param = dynamic_indices[dyn_idx->slot];
                    auto* expr = b.IndexAccessor(lhs, param);
                    return {expr, vec->type(), name};
                },  //
                [&](Default) -> ExprTypeName {
                    TINT_ICE(Transform, b.Diagnostics())
                        << "unhandled type for access chain: " << b.FriendlyName(ty);
                    return {};
                });
        } else {
            auto idx = std::get<u32>(access);
            return Switch(
                ty,  //
                [&](const sem::Struct* str) -> ExprTypeName {
                    auto* member = str->Members()[idx];
                    auto member_name = sym.NameFor(member->Name());
                    auto* expr = b.MemberAccessor(lhs, member_name);
                    ty = member->Type();
                    return {expr, ty, member_name};
                },  //
                [&](const sem::Array* arr) -> ExprTypeName {
                    auto* expr = b.IndexAccessor(lhs, idx);
                    return {expr, arr->ElemType(), std::to_string(idx)};
                },  //
                [&](const sem::Matrix* mat) -> ExprTypeName {
                    auto* expr = b.IndexAccessor(lhs, idx);
                    return {expr, mat->ColumnType(), std::to_string(idx)};
                },  //
                [&](const sem::Vector* vec) -> ExprTypeName {
                    auto* expr = b.IndexAccessor(lhs, idx);
                    return {expr, vec->type(), std::to_string(idx)};
                },  //
                [&](Default) -> ExprTypeName {
                    TINT_ICE(Transform, b.Diagnostics())
                        << "unhandled type for access chain: " << b.FriendlyName(ty);
                    return {};
                });
        }
    };

    auto build_access_chain =
        [&](const MatrixAccess& access, utils::VectorRef<const ast::Expression*> dynamic_indices,
            const std::function<const ast::Statement*(const ast::Expression*)>& build_stmt)
        -> std::pair<const ast::SwitchStatement*, std::string> {
        std::string name;
        utils::Vector<const ast::CaseStatement*, 4> cases;

        // Build switch() cases for each column of the matrix
        auto num_columns = access.matrix_type->columns();
        for (uint32_t column_idx = 0; column_idx < num_columns; column_idx++) {
            const ast::Expression* expr = b.Expr(ctx.Clone(access.var->Declaration()->symbol));
            const sem::Type* ty = access.var->Type()->UnwrapRef();
            // Build the expression up to, but not including the matrix member
            for (size_t i = 0; i < access.matrix_chain_idx - 1; i++) {
                auto [new_expr, new_ty, access_name] =
                    build_access_expr(expr, ty, access.chain[i], dynamic_indices);
                expr = new_expr;
                ty = new_ty;
                name = name + "_" + access_name;
            }
            // Get the matrix member that was dynamically accessed.
            auto mat_member_idx = std::get<u32>(access.chain[access.matrix_chain_idx - 1]);
            auto* mat_member = ty->As<sem::Struct>()->Members()[mat_member_idx];
            auto mat_columns = decomposed.mat.Get(mat_member);
            // Build the expression to the column vector member.
            expr = b.MemberAccessor(expr, (*mat_columns)[column_idx]);
            ty = mat_member->Type()->As<sem::Matrix>()->ColumnType();
            // Build the rest of the expression.
            for (size_t i = access.matrix_chain_idx; i < access.chain.Length(); i++) {
                auto [new_expr, new_ty, access_name] =
                    build_access_expr(expr, ty, access.chain[i], dynamic_indices);
                expr = new_expr;
                ty = new_ty;
                name = name + "_" + access_name;
            }

            cases.Push(b.Case(b.Expr(u32(column_idx)), b.Block(build_stmt(expr))));
        }

        auto* column_selector =
            dynamic_indices[std::get<DynamicIndex>(access.chain[access.matrix_chain_idx]).slot];
        auto* stmt = b.Switch(column_selector, std::move(cases));

        return std::make_pair(stmt, name);
    };

    auto read = [&](const sem::Expression* expr) -> const ast::Expression* {
        if (!expr) {
            return nullptr;
        }
        if (auto access = get_access(expr)) {
            // Build a helper function to load the expression chain
            auto fn = decomposed.load_store_fns.GetOrCreate(
                LoadStoreFn{access->var, access->chain, kLoad}, [&] {
                    // Build the dynamic index parameters
                    auto dynamic_index_params =
                        utils::Transform(access->dynamic_indices, [&](auto*, size_t i) {
                            return b.Param("p" + std::to_string(i), b.ty.i32());
                        });
                    auto dynamic_indices = utils::Transform(
                        dynamic_index_params, [&](auto* param) { return b.Expr(param); });

                    auto [stmt, name] = build_access_chain(access.value(), dynamic_indices,
                                                           [&](const ast::Expression* expr) {  //
                                                               return b.Return(expr);
                                                           });

                    auto fn_sym = b.Symbols().New("load_" + name);
                    auto* ret_ty = CreateASTTypeFor(ctx, expr->Type()->UnwrapRef());
                    b.Func(fn_sym, std::move(dynamic_index_params), ret_ty, utils::Vector{stmt});
                    return fn_sym;
                });

            // Build the arguments
            auto args = utils::Transform(access->dynamic_indices, [&](const sem::Expression* expr) {
                return ctx.Clone(expr->Declaration());
            });

            // Call the helper
            return b.Call(fn, std::move(args));
        } else {
            // Matrix is statically indexed. Can be emitted as an inline expression.
            auto dynamic_indices = utils::Transform(
                access->dynamic_indices,
                [&](const sem::Expression* expr) { return ctx.Clone(expr->Declaration()); });

            const ast::Expression* expr = b.Expr(ctx.Clone(access->var->Declaration()->symbol));
            const sem::Type* ty = access->var->Type()->UnwrapRef();
            for (size_t i = 0; i < access->chain.Length(); i++) {
                auto [new_expr, new_ty, _] =
                    build_access_expr(expr, ty, access->chain[i], dynamic_indices);
                expr = new_expr;
                ty = new_ty;
            }
            return expr;
        }
        return nullptr;
    };

    ctx.ReplaceAll([&](const ast::Expression* expr) { return read(sem.Get(expr)); });

    ctx.Clone();
}

}  // namespace tint::transform
