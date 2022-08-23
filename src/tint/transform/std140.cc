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

/// DynamicIndex is used by Std140::State::AccessIndex to indicate a runtime-expression index
struct DynamicIndex {
    size_t slot;  // The index of the expression in Std140::State::AccessChain::dynamic_indices
};

/// Inequality operator for DynamicIndex
bool operator!=(const DynamicIndex& a, const DynamicIndex& b) {
    return a.slot == b.slot;
}

}  // namespace

namespace tint::utils {

/// Hasher specialization for DynamicIndex
template <>
struct Hasher<DynamicIndex> {
    /// The hash function for the DynamicIndex
    /// @param d the DynamicIndex to hash
    /// @return the hash for the given DynamicIndex
    uint64_t operator()(const DynamicIndex& d) const { return utils::Hash(d.slot); }
};

}  // namespace tint::utils

namespace tint::transform {

/// The PIMPL state for the Std140 transform
struct Std140::State {
    /// Constructor
    /// @param c the CloneContext
    State(CloneContext& c) : ctx(c) {}

    /// Runs the transform
    void Run() {
        // Begin by creating forked structures for any struct that is used as a uniform buffer, that
        // either directly or transitively contains a matrix that needs splitting for std140 layout.
        ForkStructs();

        // Next, replace all the uniform variables to use the forked types.
        ReplaceUniformVarTypes();

        // Finally, replace all expression chains that used the authored types with those that
        // correctly use the forked types.
        ctx.ReplaceAll([&](const ast::Expression* expr) -> const ast::Expression* {
            if (auto access = AccessChainFor(expr)) {
                if (!access->std140_mat_idx.has_value()) {
                    // loading a std140 type, which is not a whole or partial decomposed matrix
                    return LoadWithConvert(access.value());
                }
                if (!access->IsMatrixSubset() ||  // loading a whole matrix
                    std::holds_alternative<DynamicIndex>(
                        access->indices[*access->std140_mat_idx + 1])) {
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

    /// @returns true if this transform should be run for the given program
    /// @param program the program to inspect
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
    /// Swizzle describes a vector swizzle
    using Swizzle = utils::Vector<uint32_t, 4>;

    /// AccessIndex describes a single access in an access chain.
    /// The access is one of:
    /// u32          - a static member index on a struct, static array index, static matrix column
    ///                index, static vector element index.
    /// DynamicIndex - a runtime-expression index on an array, matrix column selection, or vector
    ///                element index.
    /// Swizzle      - a static vector swizzle.
    using AccessIndex = std::variant<u32, DynamicIndex, Swizzle>;

    /// A vector of AccessIndex.
    using AccessIndices = utils::Vector<AccessIndex, 8>;

    /// A key used to cache load functions for an access chain.
    struct LoadFnKey {
        /// The root uniform buffer variable for the access chain.
        const sem::GlobalVariable* var;

        /// The chain of accesses indices.
        AccessIndices indices;

        /// Hash function for LoadFnKey.
        struct Hasher {
            /// @param fn the LoadFnKey to hash
            /// @return the hash for the given LoadFnKey
            uint64_t operator()(const LoadFnKey& fn) const {
                return utils::Hash(fn.var, fn.indices);
            }
        };

        /// Equality operator
        bool operator==(const LoadFnKey& other) const {
            return var == other.var && indices == other.indices;
        }
    };

    /// The clone context
    CloneContext& ctx;
    /// Alias to the semantic info in ctx.src
    const sem::Info& sem = ctx.src->Sem();
    /// Alias to the symbols in ctx.src
    const SymbolTable& sym = ctx.src->Symbols();
    /// Alias to the ctx.dst program builder
    ProgramBuilder& b = *ctx.dst;

    /// Map of load function signature, to the generated function
    utils::Hashmap<LoadFnKey, Symbol, 8, LoadFnKey::Hasher> load_fns;

    /// Map of std140-forked type to converter function name
    utils::Hashmap<const sem::Type*, Symbol, 8> conv_fns;

    // Uniform variables that have been modified to use a std140 type
    utils::Hashset<const sem::Variable*, 8> std140_uniforms;

    // Map of original structure to 'std140' forked structure
    utils::Hashmap<const sem::Struct*, Symbol, 8> std140_structs;

    // Map of structure member in ctx.src of a matrix type, to list of decomposed column
    // members in ctx.dst.
    utils::Hashmap<const sem::StructMember*, utils::Vector<const ast::StructMember*, 4>, 8>
        std140_mats;

    /// AccessChain describes a chain of access expressions to uniform buffer variable.
    struct AccessChain {
        /// The uniform buffer variable.
        const sem::GlobalVariable* var;
        /// The chain of access indices, starting with the first access on #var.
        AccessIndices indices;
        /// The runtime-evaluated expressions. This vector is indexed by the DynamicIndex::slot
        utils::Vector<const sem::Expression*, 8> dynamic_indices;
        /// The type of the std140-decomposed matrix being accessed.
        /// May be nullptr if the chain does not pass through a std140-decomposed matrix.
        const sem::Matrix* std140_mat_ty = nullptr;
        /// The index in #indices of the access that resolves to the std140-decomposed matrix.
        /// May hold no value if the chain does not pass through a std140-decomposed matrix.
        std::optional<size_t> std140_mat_idx;

        /// @returns true if the access chain is to part of (not the whole) std140-decomposed matrix
        bool IsMatrixSubset() const {
            return std140_mat_idx.has_value() && (std140_mat_idx.value() + 1 != indices.Length());
        }
    };

    /// @returns true if the given matrix needs decomposing to column vectors for std140 layout.
    static bool MatrixNeedsDecomposing(const sem::Matrix* mat) { return mat->ColumnStride() == 8; }

    // ForkStructs walks the structures in dependency order, forking structures that are used as
    // uniform buffers which (transitively) use matrices that need std140 decomposition to column
    // vectors.
    void ForkStructs() {
        // For each module scope declaration...
        for (auto* global : ctx.src->Sem().Module()->DependencyOrderedDeclarations()) {
            // Check to see if this is a structure used by a uniform buffer...
            auto* str = sem.Get<sem::Struct>(global);
            if (str && str->UsedAs(ast::StorageClass::kUniform)) {
                // Should this uniform buffer be forked for std140 usage?
                bool fork_std140 = false;
                utils::Vector<const ast::StructMember*, 8> members;
                for (auto* member : str->Members()) {
                    if (auto* mat = member->Type()->As<sem::Matrix>()) {
                        // Is this member a matrix that needs decomposition for std140-layout?
                        if (MatrixNeedsDecomposing(mat)) {
                            // Structure member of matrix type needs decomposition.
                            fork_std140 = true;
                            // Replace the member with column vectors.
                            const auto num_columns = mat->columns();
                            const auto name_prefix =
                                UniqueName(str->Declaration(), member->Name(), num_columns);
                            // Build a struct member for each column of the matrix
                            utils::Vector<const ast::StructMember*, 4> column_members;
                            for (uint32_t i = 0; i < num_columns; i++) {
                                utils::Vector<const ast::Attribute*, 1> attributes;
                                if ((i == 0) && mat->Align() != member->Align()) {
                                    // The matrix was @align() annotated with a larger alignment
                                    // than the natural alignment for the matrix. This extra padding
                                    // needs to be applied to the first column vector.
                                    attributes.Push(b.MemberAlign(u32(member->Align())));
                                }
                                if ((i == num_columns - 1) && mat->Size() != member->Size()) {
                                    // The matrix was @size() annotated with a larger size than the
                                    // natural size for the matrix. This extra padding needs to be
                                    // applied to the last column vector.
                                    attributes.Push(
                                        b.MemberSize(member->Size() - mat->ColumnType()->Size() *
                                                                          (num_columns - 1)));
                                }

                                // Build the member
                                const auto col_name = name_prefix + std::to_string(i);
                                const auto* col_ty = CreateASTTypeFor(ctx, mat->ColumnType());
                                const auto* col_member =
                                    ctx.dst->Member(col_name, col_ty, std::move(attributes));
                                // Add the member to the forked structure
                                members.Push(col_member);
                                // Record the member for std140_mats
                                column_members.Push(col_member);
                            }
                            std140_mats.Add(member, std::move(column_members));
                            continue;
                        }
                    }

                    // Is the member of a type that has been forked for std140-layout?
                    if (auto* std140_ty = Std140Type(member->Type())) {
                        // Yes - use this type for the forked structure member.
                        fork_std140 = true;
                        auto attrs = ctx.Clone(member->Declaration()->attributes);
                        members.Push(
                            b.Member(sym.NameFor(member->Name()), std140_ty, std::move(attrs)));
                        continue;
                    }

                    // Nothing special about this member.
                    // Push the member in src *without first cloning* to members. We'll replace this
                    // with the clone once we know whether we need for fork the structure or not.
                    members.Push(member->Declaration());
                }

                if (fork_std140) {
                    // Clone any members have not already been cloned.
                    for (auto& member : members) {
                        if (member->program_id == ctx.src->ID()) {
                            member = ctx.Clone(member);
                        }
                    }
                    auto name = b.Symbols().New(sym.NameFor(str->Name()) + "_std140");
                    auto* std140 = b.create<ast::Struct>(name, std::move(members),
                                                         ctx.Clone(str->Declaration()->attributes));
                    ctx.InsertAfter(ctx.src->AST().GlobalDeclarations(), global, std140);
                    std140_structs.Add(str, name);
                }
            }
        }
    }

    void ReplaceUniformVarTypes() {
        for (auto* global : ctx.src->AST().GlobalVariables()) {
            if (auto* var = global->As<ast::Var>()) {
                if (var->declared_storage_class == ast::StorageClass::kUniform) {
                    auto* v = sem.Get(var);
                    if (auto* std140_ty = Std140Type(v->Type()->UnwrapRef())) {
                        ctx.Replace(global->type, std140_ty);
                        std140_uniforms.Add(v);
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
                if (auto* std140 = std140_structs.Find(str)) {
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

    std::optional<AccessChain> AccessChainFor(const ast::Expression* ast_expr) {
        auto* expr = sem.Get(ast_expr);
        if (!expr) {
            return std::nullopt;
        }

        AccessChain access;

        access.var = tint::As<sem::GlobalVariable>(expr->SourceVariable());
        if (!access.var || !std140_uniforms.Contains(access.var)) {
            // Early out for expressions that aren't from a global
            return std::nullopt;
        }

        while (true) {
            enum class Action { kStop, kContinue, kError };

            Action action = Switch(
                expr,  //
                [&](const sem::VariableUser* user) {
                    if (user->Variable() == access.var) {
                        return Action::kStop;
                    }
                    if (user->Variable()->Type()->Is<sem::Pointer>()) {
                        // Walk the pointer-let chain
                        expr = user->Variable()->Constructor();
                        return Action::kContinue;
                    }
                    TINT_ICE(Transform, b.Diagnostics())
                        << "unexpected variable found walking access chain: "
                        << sym.NameFor(user->Variable()->Declaration()->symbol);
                    return Action::kError;
                },
                [&](const sem::StructMemberAccess* a) {
                    if (!access.std140_mat_ty && std140_mats.Contains(a->Member())) {
                        access.std140_mat_idx = access.indices.Length();
                        access.std140_mat_ty = expr->Type()->UnwrapRef()->As<sem::Matrix>();
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
                        access.indices.Push(s->Indices());
                    }
                    expr = s->Object();
                    return Action::kContinue;
                },
                [&](const sem::Expression* e) {
                    return Switch(e->Declaration(),  //
                                  [&](const ast::UnaryOpExpression* u) {
                                      switch (u->op) {
                                          case ast::UnaryOp::kAddressOf:
                                          case ast::UnaryOp::kIndirection:
                                              expr = sem.Get(u->expr);
                                              return Action::kContinue;
                                          default:
                                              TINT_ICE(Transform, b.Diagnostics())
                                                  << "unhandled unary op for access chain: "
                                                  << u->op;
                                              return Action::kError;
                                      }
                                  });
                },
                [&](Default) {
                    TINT_ICE(Transform, b.Diagnostics())
                        << "unhandled expression type for access chain\n"
                        << "AST: " << expr->Declaration()->TypeInfo().name << "\n"
                        << "SEM: " << expr->TypeInfo().name;
                    return Action::kError;
                });

            switch (action) {
                case Action::kContinue:
                    continue;
                case Action::kStop:
                    break;
                case Action::kError:
                    return std::nullopt;
            }

            break;
        }

        std::reverse(access.indices.begin(), access.indices.end());
        std::reverse(access.dynamic_indices.begin(), access.dynamic_indices.end());
        if (access.std140_mat_idx.has_value()) {
            access.std140_mat_idx = access.indices.Length() - *access.std140_mat_idx - 1;
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
        auto fn = conv_fns.GetOrCreate(ty, [&] {
            auto std140_ty = Std140Type(ty);
            if (!std140_ty) {
                return Symbol{};
            }

            auto* param = b.Param("val", std140_ty);

            utils::Vector<const ast::Statement*, 4> stmts;

            Switch(
                ty,  //
                [&](const sem::Struct* str) {
                    utils::Vector<const ast::Expression*, 8> args;
                    for (auto* member : str->Members()) {
                        if (auto* col_members = std140_mats.Find(member)) {
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

        if (fn.IsValid()) {
            // Call the helper
            return b.Call(fn, utils::Vector{expr});
        }

        return expr;  // Not a std140 type
    }

    const ast::Expression* LoadWithHelperFn(const AccessChain& access) {
        auto fn = load_fns.GetOrCreate(LoadFnKey{access.var, access.indices}, [&] {
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
            if (i == access.std140_mat_idx) {
                auto mat_member_idx = std::get<u32>(access.indices[i]);
                auto* mat_member = ty->As<sem::Struct>()->Members()[mat_member_idx];
                auto mat_columns = *std140_mats.Get(mat_member);
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
        auto std140_mat_idx = *access.std140_mat_idx;
        for (size_t i = 0; i < std140_mat_idx; i++) {
            auto [new_expr, new_ty, access_name] =
                BuildAccessExpr(expr, ty, access.indices[i], dynamic_index);
            expr = new_expr;
            ty = new_ty;
            name = name + "_" + access_name;
        }

        utils::Vector<const ast::Statement*, 2> stmts;

        // Get the matrix member
        auto mat_member_idx = std::get<u32>(access.indices[std140_mat_idx]);
        auto* mat_member = ty->As<sem::Struct>()->Members()[mat_member_idx];
        auto mat_columns = *std140_mats.Get(mat_member);

        auto* let = b.Let("s", b.AddressOf(expr));
        stmts.Push(b.Decl(let));
        auto columns = utils::Transform(mat_columns, [&](auto* column_member) {
            return b.MemberAccessor(b.Deref(let), column_member->symbol);
        });
        expr = b.Construct(CreateASTTypeFor(ctx, access.std140_mat_ty), std::move(columns));
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

        auto std140_mat_idx = *access.std140_mat_idx;
        auto column_param_idx = std::get<DynamicIndex>(access.indices[std140_mat_idx + 1]).slot;

        std::string name = sym.NameFor(access.var->Declaration()->symbol);
        utils::Vector<const ast::CaseStatement*, 4> cases;
        const sem::Type* ret_ty = nullptr;

        // Build switch() cases for each column of the matrix
        auto num_columns = access.std140_mat_ty->columns();
        for (uint32_t column_idx = 0; column_idx < num_columns; column_idx++) {
            utils::Vector<const ast::Statement*, 2> case_stmts;
            const ast::Expression* expr = b.Expr(ctx.Clone(access.var->Declaration()->symbol));
            const sem::Type* ty = access.var->Type()->UnwrapRef();
            // Build the expression up to, but not including the matrix member
            for (size_t i = 0; i < access.std140_mat_idx; i++) {
                auto [new_expr, new_ty, access_name] =
                    BuildAccessExpr(expr, ty, access.indices[i], dynamic_index);
                expr = new_expr;
                ty = new_ty;
                if (column_idx == 0) {
                    name = name + "_" + access_name;
                }
            }

            // Get the matrix member that was dynamically accessed.
            auto mat_member_idx = std::get<u32>(access.indices[std140_mat_idx]);
            auto* mat_member = ty->As<sem::Struct>()->Members()[mat_member_idx];
            auto mat_columns = *std140_mats.Get(mat_member);
            if (column_idx == 0) {
                name = name + "_p" + std::to_string(column_param_idx);
            }

            // Build the expression to the column vector member.
            expr = b.MemberAccessor(expr, mat_columns[column_idx]->symbol);
            ty = mat_member->Type()->As<sem::Matrix>()->ColumnType();
            // Build the rest of the expression, skipping over the column index.
            for (size_t i = std140_mat_idx + 2; i < access.indices.Length(); i++) {
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
        }
        if (auto* swizzle = std::get_if<Swizzle>(&access)) {
            return Switch(
                ty,  //
                [&](const sem::Vector* vec) -> ExprTypeName {
                    static const char xyzw[] = {'x', 'y', 'z', 'w'};
                    std::string rhs;
                    for (auto el : *swizzle) {
                        rhs += xyzw[el];
                    }
                    auto swizzle_ty = ctx.src->Types().Find<sem::Vector>(
                        vec->type(), static_cast<uint32_t>(swizzle->Length()));
                    auto* expr = b.MemberAccessor(lhs, rhs);
                    return {expr, swizzle_ty, rhs};
                },  //
                [&](Default) -> ExprTypeName {
                    TINT_ICE(Transform, b.Diagnostics())
                        << "unhandled type for access chain: " << b.FriendlyName(ty);
                    return {};
                });
        }
        // Static index
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

Std140::Std140() = default;

Std140::~Std140() = default;

bool Std140::ShouldRun(const Program* program, const DataMap&) const {
    return State::ShouldRun(program);
}

void Std140::Run(CloneContext& ctx, const DataMap&, DataMap&) const {
    State(ctx).Run();
}

}  // namespace tint::transform
