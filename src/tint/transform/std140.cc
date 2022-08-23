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
#include "src/tint/sem/module.h"
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

struct Std140::State {
    State(CloneContext& c) : ctx(c) {}

    void Run() {
        ForkStd140Structs();

        ReplaceStd140UniformVars();

        ctx.ReplaceAll([&](const ast::Expression* expr) -> const ast::Expression* {
            if (auto access = Access(expr)) {
                if (!access->matrix_indices_idx.has_value()) {
                    // loading a std140 type, which is not a whole or partial decomposed matrix
                    return LoadWithConvert(access.value());
                }
                if (!access->IsMatrixSubset() ||  // loading a whole matrix
                    std::holds_alternative<DynamicIndex>(
                        access->indices[*access->matrix_indices_idx + 1])) {
                    // Whole object or matrix is loaded, or the matrix column is indexed with a
                    // non-constant index. Build a helper function to load the expression chain.
                    return LoadWithHelperFn(access.value());
                }
                // Matrix column is statically indexed. Can be emitted as an inline expression.
                return LoadInline(access.value());
            }
            return nullptr;
        });

        ctx.Clone();
    }

    static bool ShouldRun(const Program* program) {
        for (auto* ty : program->Types()) {
            if (auto* str = ty->As<sem::Struct>()) {
                if (str->UsedAs(ast::StorageClass::kUniform)) {
                    for (auto* member : str->Members()) {
                        if (auto* mat = member->Type()->As<sem::Matrix>()) {
                            if (MatrixNeedsDecomposing(mat)) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
        return false;
    }

  private:
    enum class Action { kStop, kContinue };

    using AccessIndex = std::variant<u32, DynamicIndex>;

    using AccessIndices = utils::Vector<AccessIndex, 8>;

    struct LoadFn {
        const sem::GlobalVariable* var;
        AccessIndices access_indices;

        /// Hash function for LoadFn.
        struct Hasher {
            uint64_t operator()(const LoadFn& fn) const {
                return utils::Hash(fn.var, fn.access_indices);
            }
        };

        /// Equality operator
        bool operator==(const LoadFn& other) const {
            return var == other.var && access_indices == other.access_indices;
        }
    };

    CloneContext& ctx;
    const sem::Info& sem = ctx.src->Sem();
    const SymbolTable& sym = ctx.src->Symbols();
    ProgramBuilder& b = *ctx.dst;

    struct {
        // Uniform variables that have been split
        utils::Hashset<const sem::Variable*, 8> vars;

        // Map of original structure to 'std140' forked structure
        utils::Hashmap<const sem::Struct*, Symbol, 8> structs;

        // Map of structure member in ctx.src of a matrix type, to list of decomposed column
        // members in ctx.dst.
        utils::Hashmap<const sem::StructMember*, utils::Vector<const ast::StructMember*, 4>, 8> mat;

        /// Map of load function signature, to the generated function
        utils::Hashmap<LoadFn, Symbol, 8, LoadFn::Hasher> load_fns;

        /// Map of std140-forked type to converter function name
        utils::Hashmap<const sem::Type*, Symbol, 8> conv_fns;

    } decomposed;

    struct AccessChain {
        const sem::GlobalVariable* var;
        AccessIndices indices;
        utils::Vector<const sem::Expression*, 8> dynamic_indices;

        // The type of the matrix being indexed
        const sem::Matrix* matrix_type = nullptr;

        // The index in indices, of the access that resolves to the decomposed matrix
        std::optional<size_t> matrix_indices_idx;

        /// @returns true if the access chain is to part of (not the whole) matrix
        bool IsMatrixSubset() const {
            return matrix_indices_idx.has_value() &&
                   (matrix_indices_idx.value() + 1 != indices.Length());
        }
    };

    static bool MatrixNeedsDecomposing(const sem::Matrix* mat) { return mat->ColumnStride() == 8; }

    void ForkStd140Structs() {
        // Walk the structures in dependency order, forking structures that are used as uniform
        // buffers which (transitively) use matrices that need decomposition.
        for (auto* global : ctx.src->Sem().Module()->DependencyOrderedDeclarations()) {
            auto* str = sem.Get<sem::Struct>(global);
            if (str && str->UsedAs(ast::StorageClass::kUniform)) {
                bool fork_std140 = false;
                utils::Vector<const ast::StructMember*, 8> members;
                for (auto* member : str->Members()) {
                    if (auto* mat = member->Type()->As<sem::Matrix>()) {
                        if (MatrixNeedsDecomposing(mat)) {
                            fork_std140 = true;
                            // Structure member of matrix type needs decomposition.
                            // Replace the member with column vectors.
                            const auto num_columns = mat->columns();
                            const auto name =
                                UniqueName(str->Declaration(), member->Name(), num_columns);
                            utils::Vector<const ast::StructMember*, 4> column_members;

                            for (uint32_t i = 0; i < num_columns; i++) {
                                utils::Vector<const ast::Attribute*, 1> attributes;
                                if ((i == num_columns - 1) && mat->Size() != member->Size()) {
                                    // The matrix was @size() annotated with a larger size than the
                                    // natural size for the matrix. This extra padding needs to be
                                    // applied to the last column vector.
                                    attributes.Push(
                                        b.MemberSize(member->Size() - mat->ColumnType()->Size() *
                                                                          (num_columns - 1)));
                                }

                                const auto col_name = name + std::to_string(i);
                                const auto col_sym = ctx.dst->Symbols().New(col_name);
                                const auto* col_ty = CreateASTTypeFor(ctx, mat->ColumnType());
                                const auto* col_member =
                                    ctx.dst->Member(col_sym, col_ty, std::move(attributes));
                                // Record the column vector member names
                                column_members.Push(col_member);
                                members.Push(col_member);
                            }
                            decomposed.mat.Add(member, std::move(column_members));
                            continue;
                        }
                    }
                    if (auto* std140_ty = Std140Type(member->Type())) {
                        fork_std140 = true;
                        auto attrs = ctx.Clone(member->Declaration()->attributes);
                        members.Push(
                            b.Member(sym.NameFor(member->Name()), std140_ty, std::move(attrs)));
                        continue;
                    }
                    members.Push(ctx.Clone(member->Declaration()));
                }
                if (fork_std140) {
                    auto name = b.Symbols().New(sym.NameFor(str->Name()) + "_std140");
                    auto* std140 = b.create<ast::Struct>(name, std::move(members),
                                                         ctx.Clone(str->Declaration()->attributes));
                    ctx.InsertAfter(ctx.src->AST().GlobalDeclarations(), global, std140);
                    decomposed.structs.Add(str, name);
                }
            }
        }
    }

    void ReplaceStd140UniformVars() {
        for (auto* global : ctx.src->AST().GlobalVariables()) {
            if (auto* var = global->As<ast::Var>()) {
                if (var->declared_storage_class == ast::StorageClass::kUniform) {
                    auto* v = sem.Get(var);
                    if (auto* std140_ty = Std140Type(v->Type()->UnwrapRef())) {
                        ctx.Replace(global->type, std140_ty);
                        decomposed.vars.Add(v);
                    }
                }
            }
        }
    }

    std::string UniqueName(const ast::Struct* str, Symbol unsuffixed, uint32_t count) const {
        auto prefix = sym.NameFor(unsuffixed);
        while (true) {
            prefix += "_";

            utils::Hashset<std::string, 4> strings;
            for (uint32_t i = 0; i < count; i++) {
                strings.Add(prefix + std::to_string(i));
            }

            bool unique = true;
            for (auto* member : str->members) {
                if (strings.Contains(sym.NameFor(member->symbol))) {
                    unique = false;
                    break;
                }
            }

            if (unique) {
                return prefix;
            }
        }
    }

    const ast::Type* Std140Type(const sem::Type* ty) const {
        return Switch(
            ty,  //
            [&](const sem::Struct* str) -> const ast::Type* {
                if (auto* std140 = decomposed.structs.Find(str)) {
                    return b.create<ast::TypeName>(*std140);
                }
                return nullptr;
            },
            [&](const sem::Array* arr) -> const ast::Type* {
                if (auto* std140 = Std140Type(arr->ElemType())) {
                    utils::Vector<const ast::Attribute*, 1> attrs;
                    if (!arr->IsStrideImplicit()) {
                        attrs.Push(ctx.dst->create<ast::StrideAttribute>(arr->Stride()));
                    }
                    return b.create<ast::Array>(std140, b.Expr(u32(arr->Count())),
                                                std::move(attrs));
                }
                return nullptr;
            });
    }

    std::optional<AccessChain> Access(const ast::Expression* ast_expr) {
        auto* expr = sem.Get(ast_expr);
        if (!expr) {
            return std::nullopt;
        }

        AccessChain access;

        access.var = tint::As<sem::GlobalVariable>(expr->SourceVariable());
        if (!access.var || !decomposed.vars.Contains(access.var)) {
            // Early out for expressions that aren't from a global
            return std::nullopt;
        }

        while (true) {
            auto action = Switch(
                expr,  //
                [&](const sem::VariableUser*) { return Action::kStop; },
                [&](const sem::StructMemberAccess* a) {
                    if (!access.matrix_type && decomposed.mat.Contains(a->Member())) {
                        access.matrix_indices_idx = access.indices.Length();
                        access.matrix_type = expr->Type()->UnwrapRef()->As<sem::Matrix>();
                    }
                    access.indices.Push(u32(a->Member()->Index()));
                    expr = a->Object();
                    return Action::kContinue;
                },
                [&](const sem::IndexAccessorExpression* a) {
                    if (auto* val = a->Index()->ConstantValue()) {
                        access.indices.Push(val->As<u32>());
                    } else {
                        access.indices.Push(DynamicIndex{});
                        access.dynamic_indices.Push(a->Index());
                    }
                    expr = a->Object();
                    return Action::kContinue;
                },
                [&](const sem::Swizzle* s) {
                    if (s->Indices().Length() == 1) {
                        access.indices.Push(u32(s->Indices()[0]));
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

        std::reverse(access.indices.begin(), access.indices.end());
        std::reverse(access.dynamic_indices.begin(), access.dynamic_indices.end());
        if (access.matrix_indices_idx.has_value()) {
            access.matrix_indices_idx = access.indices.Length() - *access.matrix_indices_idx - 1;
        }

        return access;
    }

    const std::string ConvertSuffix(const sem::Type* ty) {
        return Switch(
            ty,  //
            [&](const sem::Struct* str) { return sym.NameFor(str->Name()); },
            [&](const sem::Array* arr) {
                return "arr_" + std::to_string(arr->Count()) + "_" + ConvertSuffix(arr->ElemType());
            },
            [&](Default) {
                TINT_ICE(Transform, b.Diagnostics())
                    << "unhandled type for conversion name: " << b.FriendlyName(ty);
                return "";
            });
    }

    const ast::Expression* LoadWithConvert(const AccessChain& access) {
        const ast::Expression* expr = b.Expr(sym.NameFor(access.var->Declaration()->symbol));
        const sem::Type* ty = access.var->Type()->UnwrapRef();
        auto dynamic_index = [&](size_t idx) {
            return ctx.Clone(access.dynamic_indices[idx]->Declaration());
        };
        for (auto index : access.indices) {
            auto [new_expr, new_ty, _] = BuildAccessExpr(expr, ty, index, dynamic_index);
            expr = new_expr;
            ty = new_ty;
        }
        return Convert(ty, expr);
    }

    const ast::Expression* Convert(const sem::Type* ty, const ast::Expression* expr) {
        auto std140_ty = Std140Type(ty);
        if (!std140_ty) {
            return expr;
        }

        auto* param = b.Param("val", std140_ty);
        auto fn = decomposed.conv_fns.GetOrCreate(ty, [&] {
            utils::Vector<const ast::Statement*, 4> stmts;

            Switch(
                ty,  //
                [&](const sem::Struct* str) {
                    utils::Vector<const ast::Expression*, 8> args;
                    for (auto* member : str->Members()) {
                        if (auto* col_members = decomposed.mat.Find(member)) {
                            auto* mat_ty = CreateASTTypeFor(ctx, member->Type());
                            auto mat_args =
                                utils::Transform(*col_members, [&](const ast::StructMember* m) {
                                    return b.MemberAccessor(param, m->symbol);
                                });
                            args.Push(b.Construct(mat_ty, std::move(mat_args)));
                        } else {
                            auto* conv =
                                Convert(member->Type(),
                                        b.MemberAccessor(param, sym.NameFor(member->Name())));
                            args.Push(conv);
                        }
                    }
                    auto* converted = b.Construct(CreateASTTypeFor(ctx, ty), std::move(args));
                    stmts.Push(b.Return(converted));
                },  //
                [&](const sem::Array* arr) {
                    auto* var = b.Var("arr", CreateASTTypeFor(ctx, ty));
                    auto* i = b.Var("i", b.ty.u32());
                    auto* dst_el = b.IndexAccessor(var, i);
                    auto* src_el = Convert(arr->ElemType(), b.IndexAccessor(param, i));
                    stmts.Push(b.Decl(var));
                    stmts.Push(b.For(b.Decl(i),                         //
                                     b.LessThan(i, u32(arr->Count())),  //
                                     b.Increment(i),                    //
                                     b.Block(b.Assign(dst_el, src_el))));
                    stmts.Push(b.Return(var));
                },
                [&](Default) {
                    TINT_ICE(Transform, b.Diagnostics())
                        << "unhandled type for conversion: " << b.FriendlyName(ty);
                });

            auto* ret_ty = CreateASTTypeFor(ctx, ty);
            auto fn_sym = b.Symbols().New("conv_" + ConvertSuffix(ty));
            b.Func(fn_sym, utils::Vector{param}, ret_ty, std::move(stmts));
            return fn_sym;
        });

        // Call the helper
        return b.Call(fn, utils::Vector{expr});
    }

    const ast::Expression* LoadWithHelperFn(const AccessChain& access) {
        auto fn = decomposed.load_fns.GetOrCreate(LoadFn{access.var, access.indices}, [&] {
            if (access.IsMatrixSubset()) {
                return BuildLoadPartialMatrixFn(access,
                                                [&](const ast::Expression* e) {  //
                                                    return b.Return(e);
                                                });
            }
            return BuildLoadMatrixFn(access);
        });

        // Build the arguments
        auto args = utils::Transform(access.dynamic_indices, [&](const sem::Expression* e) {
            return b.Construct(b.ty.u32(), ctx.Clone(e->Declaration()));
        });

        // Call the helper
        return b.Call(fn, std::move(args));
    }

    const ast::Expression* LoadInline(const AccessChain& access) {
        const ast::Expression* expr = b.Expr(ctx.Clone(access.var->Declaration()->symbol));
        const sem::Type* ty = access.var->Type()->UnwrapRef();
        for (size_t i = 0; i < access.indices.Length(); i++) {
            if (i == access.matrix_indices_idx) {
                auto mat_member_idx = std::get<u32>(access.indices[i]);
                auto* mat_member = ty->As<sem::Struct>()->Members()[mat_member_idx];
                auto mat_columns = *decomposed.mat.Get(mat_member);
                auto column_idx = std::get<u32>(access.indices[i + 1]);
                expr = b.MemberAccessor(expr, mat_columns[column_idx]->symbol);
                ty = mat_member->Type()->As<sem::Matrix>()->ColumnType();
                i++;  // Skip over the column access
            } else {
                auto dynamic_index = [&](size_t idx) {
                    return ctx.Clone(access.dynamic_indices[idx]->Declaration());
                };
                auto [new_expr, new_ty, _] =
                    BuildAccessExpr(expr, ty, access.indices[i], dynamic_index);
                expr = new_expr;
                ty = new_ty;
            }
        }
        return expr;
    }

    Symbol BuildLoadMatrixFn(const AccessChain& access) {
        // Build the dynamic index parameters
        auto dynamic_index_params = utils::Transform(access.dynamic_indices, [&](auto*, size_t i) {
            return b.Param("p" + std::to_string(i), b.ty.u32());
        });
        auto dynamic_index = [&](size_t idx) { return b.Expr(dynamic_index_params[idx]->symbol); };

        const ast::Expression* expr = b.Expr(ctx.Clone(access.var->Declaration()->symbol));
        std::string name = sym.NameFor(access.var->Declaration()->symbol);
        const sem::Type* ty = access.var->Type()->UnwrapRef();

        // Build the expression up to, but not including the matrix member
        auto matrix_indices_idx = *access.matrix_indices_idx;
        for (size_t i = 0; i < matrix_indices_idx; i++) {
            auto [new_expr, new_ty, access_name] =
                BuildAccessExpr(expr, ty, access.indices[i], dynamic_index);
            expr = new_expr;
            ty = new_ty;
            name = name + "_" + access_name;
        }

        utils::Vector<const ast::Statement*, 2> stmts;

        // Get the matrix member
        auto mat_member_idx = std::get<u32>(access.indices[matrix_indices_idx]);
        auto* mat_member = ty->As<sem::Struct>()->Members()[mat_member_idx];
        auto mat_columns = *decomposed.mat.Get(mat_member);

        auto* let = b.Let("s", b.AddressOf(expr));
        stmts.Push(b.Decl(let));
        auto columns = utils::Transform(mat_columns, [&](auto* column_member) {
            return b.MemberAccessor(b.Deref(let), column_member->symbol);
        });
        expr = b.Construct(CreateASTTypeFor(ctx, access.matrix_type), std::move(columns));
        ty = mat_member->Type();
        name = name + "_" + sym.NameFor(mat_member->Name());

        stmts.Push(b.Return(expr));

        auto* ret_ty = CreateASTTypeFor(ctx, ty);
        auto fn_sym = b.Symbols().New("load_" + name);
        b.Func(fn_sym, std::move(dynamic_index_params), ret_ty, std::move(stmts));
        return fn_sym;
    }

    Symbol BuildLoadPartialMatrixFn(
        const AccessChain& access,
        const std::function<const ast::Statement*(const ast::Expression*)>& build_stmt) {
        // Build the dynamic index parameters
        auto dynamic_index_params = utils::Transform(access.dynamic_indices, [&](auto*, size_t i) {
            return b.Param("p" + std::to_string(i), b.ty.u32());
        });
        auto dynamic_index = [&](size_t idx) { return b.Expr(dynamic_index_params[idx]->symbol); };

        auto matrix_indices_idx = *access.matrix_indices_idx;
        auto column_param_idx = std::get<DynamicIndex>(access.indices[matrix_indices_idx + 1]).slot;

        std::string name = sym.NameFor(access.var->Declaration()->symbol);
        utils::Vector<const ast::CaseStatement*, 4> cases;
        const sem::Type* ret_ty = nullptr;

        // Build switch() cases for each column of the matrix
        auto num_columns = access.matrix_type->columns();
        for (uint32_t column_idx = 0; column_idx < num_columns; column_idx++) {
            utils::Vector<const ast::Statement*, 2> case_stmts;
            const ast::Expression* expr = b.Expr(ctx.Clone(access.var->Declaration()->symbol));
            const sem::Type* ty = access.var->Type()->UnwrapRef();
            // Build the expression up to, but not including the matrix member
            for (size_t i = 0; i < access.matrix_indices_idx; i++) {
                auto [new_expr, new_ty, access_name] =
                    BuildAccessExpr(expr, ty, access.indices[i], dynamic_index);
                expr = new_expr;
                ty = new_ty;
                if (column_idx == 0) {
                    name = name + "_" + access_name;
                }
            }

            // Get the matrix member that was dynamically accessed.
            auto mat_member_idx = std::get<u32>(access.indices[matrix_indices_idx]);
            auto* mat_member = ty->As<sem::Struct>()->Members()[mat_member_idx];
            auto mat_columns = *decomposed.mat.Get(mat_member);
            if (column_idx == 0) {
                name = name + "_p" + std::to_string(column_param_idx);
            }

            // Build the expression to the column vector member.
            expr = b.MemberAccessor(expr, mat_columns[column_idx]->symbol);
            ty = mat_member->Type()->As<sem::Matrix>()->ColumnType();
            // Build the rest of the expression, skipping over the column index.
            for (size_t i = matrix_indices_idx + 2; i < access.indices.Length(); i++) {
                auto [new_expr, new_ty, access_name] =
                    BuildAccessExpr(expr, ty, access.indices[i], dynamic_index);
                expr = new_expr;
                ty = new_ty;
                if (column_idx == 0) {
                    name = name + "_" + access_name;
                }
            }

            case_stmts.Push(build_stmt(expr));
            cases.Push(b.Case(b.Expr(u32(column_idx)), b.Block(std::move(case_stmts))));

            if (column_idx == 0) {
                ret_ty = ty;
            }
        }

        // Build the default case
        cases.Push(b.DefaultCase(b.Block(b.Return(b.Construct(CreateASTTypeFor(ctx, ret_ty))))));

        auto* column_selector = dynamic_index(column_param_idx);
        auto* stmt = b.Switch(column_selector, std::move(cases));

        auto fn_sym = b.Symbols().New("load_" + name);
        b.Func(fn_sym, std::move(dynamic_index_params), CreateASTTypeFor(ctx, ret_ty),
               utils::Vector{stmt});
        return fn_sym;
    }

    struct ExprTypeName {
        const ast::Expression* expr;
        const sem::Type* type;
        std::string name;
    };

    ExprTypeName BuildAccessExpr(const ast::Expression* lhs,
                                 const sem::Type* ty,
                                 AccessIndex access,
                                 std::function<const ast::Expression*(size_t)> dynamic_index) {
        if (auto* dyn_idx = std::get_if<DynamicIndex>(&access)) {
            auto name = "p" + std::to_string(dyn_idx->slot);
            return Switch(
                ty,  //
                [&](const sem::Array* arr) -> ExprTypeName {
                    auto* idx = dynamic_index(dyn_idx->slot);
                    auto* expr = b.IndexAccessor(lhs, idx);
                    return {expr, arr->ElemType(), name};
                },  //
                [&](const sem::Matrix* mat) -> ExprTypeName {
                    auto* idx = dynamic_index(dyn_idx->slot);
                    auto* expr = b.IndexAccessor(lhs, idx);
                    return {expr, mat->ColumnType(), name};
                },  //
                [&](const sem::Vector* vec) -> ExprTypeName {
                    auto* idx = dynamic_index(dyn_idx->slot);
                    auto* expr = b.IndexAccessor(lhs, idx);
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
    }
};

Std140::Std140() = default;

Std140::~Std140() = default;

bool Std140::ShouldRun(const Program* program, const DataMap&) const {
    return State::ShouldRun(program);
}

void Std140::Run(CloneContext& ctx, const DataMap&, DataMap&) const {
    State(ctx).Run();
}

}  // namespace tint::transform
