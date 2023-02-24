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

#include "src/tint/transform/packed_vec3.h"

#include <algorithm>
#include <string>
#include <utility>

#include "src/tint/ast/assignment_statement.h"
#include "src/tint/builtin/builtin.h"
#include "src/tint/program_builder.h"
#include "src/tint/sem/array_count.h"
#include "src/tint/sem/index_accessor_expression.h"
#include "src/tint/sem/load.h"
#include "src/tint/sem/member_accessor_expression.h"
#include "src/tint/sem/statement.h"
#include "src/tint/sem/type_expression.h"
#include "src/tint/sem/variable.h"
#include "src/tint/type/array.h"
#include "src/tint/type/reference.h"
#include "src/tint/type/vector.h"
#include "src/tint/utils/hashmap.h"
#include "src/tint/utils/hashset.h"
#include "src/tint/utils/vector.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::PackedVec3);

using namespace tint::number_suffixes;  // NOLINT

namespace tint::transform {

/// PIMPL state for the transform
struct PackedVec3::State {
    /// Constructor
    /// @param program the source program
    explicit State(const Program* program) : src(program) {}

    /// WrapperStructKey is the hashmap key to a packed vec3 wrapper struct.
    struct WrapperStructKey {
        const type::Type* type = nullptr;  // The vec3 type.
        bool array_element = false;        // Is it used as an array element?
        bool operator==(const WrapperStructKey& rhs) const {
            return type == rhs.type && array_element == rhs.array_element;
        }
        struct Hasher {
            inline std::size_t operator()(const WrapperStructKey& u) const {
                return utils::Hash(u.type, u.array_element);
            }
        };
    };

    /// The name of the struct member used when wrapping packed vec3 types.
    static constexpr const char* kStructMemberName = "elements";

    /// TODO: update name and comment
    /// The names of the structures used to wrap packed vec3 types used in arrays.
    utils::Hashmap<WrapperStructKey, Symbol, 4, WrapperStructKey::Hasher>
        array_element_struct_names;

    /// A cache of host-shareable structures that have been rewritten.
    utils::Hashmap<const type::Type*, Symbol, 4> rewritten_structs;

    /// A map from type to the name of a helper function used to pack that type.
    utils::Hashmap<const type::Type*, Symbol, 4> pack_helpers;

    /// A map from type to the name of a helper function used to unpack that type.
    utils::Hashmap<const type::Type*, Symbol, 4> unpack_helpers;

    /// @param ty the type to test
    /// @returns true if `ty` is a vec3, false otherwise
    bool IsVec3(const type::Type* ty) {
        if (auto* vec = ty->As<type::Vector>()) {
            if (vec->Width() == 3 && !vec->type()->Is<type::Bool>()) {
                return true;
            }
        }
        return false;
    }

    /// @param ty the type to test
    /// @returns true if `ty` is or contains a vec3, false otherwise
    bool ContainsVec3(const type::Type* ty) {
        return Switch(
            ty,  //
            [&](const type::Vector* vec) { return IsVec3(vec); },
            [&](const type::Matrix* mat) { return ContainsVec3(mat->ColumnType()); },
            [&](const type::Array* arr) { return ContainsVec3(arr->ElemType()); },
            [&](const type::Struct* str) {
                for (auto* member : str->Members()) {
                    if (ContainsVec3(member->Type())) {
                        return true;
                    }
                }
                return false;
            });
    }

    /// Create a `__packed_vec3` type with the same element type as `ty`.
    /// @param ty a three-element vector type
    /// @returns the new AST type
    ast::Type MakePackedVec3(const type::Type* ty) {
        auto* vec = ty->As<type::Vector>();
        TINT_ASSERT(Transform, vec != nullptr && vec->Width() == 3);
        return b.ty(utils::ToString(builtin::Builtin::kPackedVec3),
                    CreateASTTypeFor(ctx, vec->type()));
    }

    /// TODO: Update comment and name
    /// Rewrite an array or matrix type using `__packed_vec3` if needed.
    /// Wraps the `__packed_vec3` in a struct to give it the 16-byte alignment it needs when
    /// contained inside an array. Matrices with three rows become arrays of columns.
    /// @param ty the type to rewrite
    /// @param array_element `true` if this is being called for the element of an array
    /// @returns the new AST type, or nullptr if rewriting was not necessary
    ast::Type RewriteArrayType(const type::Type* ty, bool array_element = false) {
        return Switch(
            ty,
            [&](const type::Vector* vec) -> ast::Type {
                if (IsVec3(vec)) {
                    WrapperStructKey key{vec, array_element};
                    return b.ty(array_element_struct_names.GetOrCreate(key, [&]() {
                        // Create a struct with a single `__packed_vec3` member.
                        auto name = b.Symbols().New(
                            "tint_packed_vec3_" + vec->type()->FriendlyName(src->Symbols()) +
                            (array_element ? "_array_element" : "_struct_member"));
                        // TODO: add attribute to align it, if in array (or if not struct)
                        // TODO: would need two different structs then - one aligned one not
                        utils::Vector<const ast::Attribute*, 1> attributes;
                        if (array_element) {
                            attributes.Push(b.MemberAlign(AInt(vec->Align())));
                        }
                        auto* member =
                            b.Member(kStructMemberName, MakePackedVec3(vec), std::move(attributes));
                        b.Structure(b.Ident(name), utils::Vector{member}, utils::Empty);
                        return name;
                    }));
                }
                return {};
            },
            [&](const type::Matrix* mat) -> ast::Type {
                auto new_col_type = RewriteArrayType(mat->ColumnType(), /* array_element */ true);
                if (new_col_type) {
                    // Rewrite the matrix as an array of `tint_vec3_struct`.
                    return b.ty.array(new_col_type, u32(mat->columns()));
                }
                return {};
            },
            [&](const type::Array* arr) -> ast::Type {
                auto new_type = RewriteArrayType(arr->ElemType(), /* array_element */ true);
                if (new_type) {
                    // Rewrite the array with the modified element type.
                    utils::Vector<const ast::Attribute*, 1> attrs;
                    if (!arr->IsStrideImplicit()) {
                        attrs.Push(b.Stride(arr->Stride()));
                    }
                    if (arr->Count()->Is<type::RuntimeArrayCount>()) {
                        return b.ty.array(new_type, std::move(attrs));
                    } else if (auto count = arr->ConstantCount()) {
                        return b.ty.array(new_type, u32(count.value()), std::move(attrs));
                    } else {
                        TINT_ICE(Transform, b.Diagnostics())
                            << type::Array::kErrExpectedConstantCount;
                        return {};
                    }
                }
                return {};
            },
            [&](const sem::Struct* str) -> ast::Type {
                // TODO: need these checks here, or done outside?
                if (ContainsVec3(str) && str->IsHostShareable()) {
                    auto name = rewritten_structs.GetOrCreate(str, [&]() {
                        utils::Vector<const ast::StructMember*, 4> members;
                        for (auto* member : str->Members()) {
                            // If the member type contains a vec3, rewrite it.
                            auto new_type = RewriteArrayType(member->Type());
                            if (new_type) {
                                // Copy the member attributes.
                                bool has_explicit_align = false;
                                utils::Vector<const ast::Attribute*, 4> attributes;
                                for (auto* attr : member->Declaration()->attributes) {
                                    if (attr->Is<ast::StructMemberAlignAttribute>()) {
                                        has_explicit_align = true;
                                    }
                                    attributes.Push(ctx.Clone(attr));
                                }
                                // If the alignment wasn't already specified, add an attribute to
                                // make sure that we don't alter the alignment when using the packed
                                // vector type.
                                if (!has_explicit_align) {
                                    attributes.Push(b.MemberAlign(AInt(member->Align())));
                                }
                                members.Push(b.Member(ctx.Clone(member->Name()), new_type,
                                                      std::move(attributes)));
                            } else {
                                // No vec3s, just clone the member as is.
                                members.Push(ctx.Clone(member->Declaration()));
                            }
                        }
                        auto struct_name = b.Symbols().New(
                            src->Symbols().NameFor(str->Declaration()->name->symbol) +
                            "_tint_packed_vec3");
                        b.Structure(struct_name, std::move(members));
                        return struct_name;
                    });
                    return b.ty(name);
                }
                return {};
            });
    }

    /// Create a helper function to recursively pack or unpack an array that contains vec3 types.
    /// @param name_prefix the name of the helper function
    /// @param ty the array or matrix type to pack or unpack
    /// @param pack_or_unpack_element a function that packs or unpacks an element with a given type
    /// @param in_type a function that create an AST type for the input type
    /// @param out_type a function that create an AST type for the output type
    /// @returns the name of the helper function
    Symbol MakePackUnpackHelper(
        const char* name_prefix,
        const type::Type* ty,
        const std::function<const ast::Expression*(const ast::Expression*,
                                                   const type::Type*,
                                                   bool array_element)>& pack_or_unpack_element,
        const std::function<ast::Type()>& in_type,
        const std::function<ast::Type()>& out_type) {
        // Determine the number of array elements and the element type.
        uint32_t num_elements = 0;
        const type::Type* element_type = nullptr;
        // TODO: switch?
        if (auto* arr = ty->As<type::Array>()) {
            TINT_ASSERT(Transform, arr->ConstantCount());
            num_elements = arr->ConstantCount().value();
            element_type = arr->ElemType();
        } else if (auto* mat = ty->As<type::Matrix>()) {
            num_elements = mat->columns();
            element_type = mat->ColumnType();
        } else if (auto* str = ty->As<sem::Struct>()) {
            // Generate an expression for packing or unpacking an element of the array.
            // auto* element = pack_or_unpack_element(b.IndexAccessor("in", "i"), element_type);

            // TODO: update comment
            // Generate the body of the function:
            // fn helper(in : in_type) -> out_type {
            //   var result : out_type;
            //   for (var i = 0u; i < num_elements; i = i + 1) {
            //     result[i] = pack_or_unpack_element(in[i]);
            //   }
            //   return result;
            // }
            utils::Vector<const ast::Statement*, 4> statements;
            statements.Push(b.Decl(b.Var("result", out_type())));
            for (auto* member : str->Members()) {
                auto* element = pack_or_unpack_element(
                    b.MemberAccessor("in", b.Ident(ctx.Clone(member->Name()))), member->Type(),
                    /* array_element */ false);
                statements.Push(b.Assign(
                    b.MemberAccessor("result", b.Ident(ctx.Clone(member->Name()))), element));
            }
            statements.Push(b.Return("result"));
            auto name = b.Symbols().New(name_prefix);
            b.Func(name, utils::Vector{b.Param("in", in_type())}, out_type(),
                   std::move(statements));
            return name;
        }

        // Generate an expression for packing or unpacking an element of the array.
        auto* element = pack_or_unpack_element(b.IndexAccessor("in", "i"), element_type,
                                               /* array_element */ true);

        // Generate the body of the function:
        // fn helper(in : in_type) -> out_type {
        //   var result : out_type;
        //   for (var i = 0u; i < num_elements; i = i + 1) {
        //     result[i] = pack_or_unpack_element(in[i]);
        //   }
        //   return result;
        // }
        utils::Vector<const ast::Statement*, 4> statements;
        statements.Push(b.Decl(b.Var("result", out_type())));
        statements.Push(b.For(                   //
            b.Decl(b.Var("i", b.ty.u32())),      //
            b.LessThan("i", u32(num_elements)),  //
            b.Assign("i", b.Add("i", 1_a)),      //
            b.Block(utils::Vector{
                b.Assign(b.IndexAccessor("result", "i"), element),
            })));
        statements.Push(b.Return("result"));
        auto name = b.Symbols().New(name_prefix);
        b.Func(name, utils::Vector{b.Param("in", in_type())}, out_type(), std::move(statements));
        return name;
    }

    /// Unpack the array value `expr` to the unpacked type `ty`. If `ty` is a matrix, this will
    /// produce a regular matNx3 value from an array of packed column vectors.
    /// @param expr the array value expression to unpack
    /// @param ty the unpacked type
    /// @returns an expression that holds the unpacked value
    const ast::Expression* UnpackArray(const ast::Expression* expr, const type::Type* ty) {
        auto helper = unpack_helpers.GetOrCreate(ty, [&]() {
            return MakePackUnpackHelper(
                "unpack_array", ty,
                [&](auto* element, const type::Type* element_type, bool) -> const ast::Expression* {
                    if (element_type->Is<type::Vector>()) {
                        // Unpack a vector element by extracting the member from the wrapper struct
                        // and then casting it to a regular vec3.
                        return b.Call(CreateASTTypeFor(ctx, element_type),
                                      b.MemberAccessor(element, kStructMemberName));
                    } else {
                        return UnpackArray(element, element_type);
                    }
                },
                [&]() { return RewriteArrayType(ty); },
                [&]() { return CreateASTTypeFor(ctx, ty); });
        });
        return b.Call(helper, expr);
    }

    /// Pack the array value `expr` from the unpacked type `ty`. If `ty` is a matrix, this will
    /// produce an array of packed column vectors.
    /// @param expr the array or matrix value expression to pack
    /// @param ty the unpacked type
    /// @returns an expression that holds the packed value
    const ast::Expression* PackArray(const ast::Expression* expr, const type::Type* ty) {
        auto helper = pack_helpers.GetOrCreate(ty, [&]() {
            return MakePackUnpackHelper(
                "pack_array", ty,
                [&](auto* element, const type::Type* element_type,
                    bool array_element) -> const ast::Expression* {
                    if (element_type->Is<type::Vector>()) {
                        // Pack a vector element by casting it to a packed_vec3 and then
                        // constructing a wrapper struct.
                        return b.Call(RewriteArrayType(element_type, array_element),
                                      b.Call(MakePackedVec3(element_type), element));
                    } else {
                        return PackArray(element, element_type);
                    }
                },
                [&]() { return CreateASTTypeFor(ctx, ty); },
                [&]() { return RewriteArrayType(ty); });
        });
        return b.Call(helper, expr);
    }

    /// @returns true if there are host-shareable vec3's that need transforming
    bool ShouldRun() {
        // Check for vec3s in the types of all uniform and storage buffer variables to determine
        // if the transform is necessary.
        for (auto* decl : src->AST().GlobalVariables()) {
            auto* var = sem.Get<sem::GlobalVariable>(decl);
            if (var && builtin::IsHostShareable(var->AddressSpace()) &&
                ContainsVec3(var->Type()->UnwrapRef())) {
                return true;
            }
        }
        return false;
    }

    /// Runs the transform
    /// @returns the new program or SkipTransform if the transform is not required
    ApplyResult Run() {
        if (!ShouldRun()) {
            return SkipTransform;
        }

        // Changing the types of certain structure members can trigger stricter layout validation
        // rules for the uniform address space. In particular, replacing 16-bit matrices with arrays
        // violates the requirement that the array element stride is a multiple of 16 bytes, and
        // replacing vec3s with a structure violates the requirement that there must be at least 16
        // bytes from the start of a structure to the start of the next member.
        // Disable these validation rules using an internal extension, as MSL does not have these
        // restrictions.
        b.Enable(builtin::Extension::kChromiumInternalRelaxedUniformLayout);

        // Track expressions that need to be packed or unpacked.
        utils::Hashset<const sem::ValueExpression*, 8> to_pack;
        utils::Hashset<const sem::ValueExpression*, 8> to_unpack;

        // Helper to record a pack operation for an expression if it contains a vec3.
        auto pack_if_needed = [&](const sem::ValueExpression* expr) {
            if (!expr || !ContainsVec3(expr->Type())) {
                // Nothing to do.
                return;
            }
            if (to_unpack.Contains(expr)) {
                // The expression will already be packed, so skip the pending unpack.
                to_unpack.Remove(expr);
                if (IsVec3(expr->Type()) &&
                    expr->UnwrapLoad()
                        ->IsAnyOf<sem::IndexAccessorExpression, sem::StructMemberAccess>()) {
                    // TODO: and direct too, if we wrap that?
                    // TODO: and update comment
                    // If the expression produces a vec3 from an array index expression, extract the
                    // packed vector from the wrapper struct.
                    ctx.Replace(
                        expr->Declaration(),
                        b.MemberAccessor(ctx.Clone(expr->Declaration()), kStructMemberName));
                }
            } else if (expr) {
                to_pack.Add(expr);
            }
        };

        /// TODO: could potentially unify the struct member and array element logic by always
        // creating a wrapper struct, but making the member aligned only for the array case (and
        // making the outer member aligned in the struct case).

        /// TODO: update comment to talk about multi-versioning structs
        // Replace vec3 types with `__packed_vec3` types by rewriting type specifiers and inserting
        // code to pack or unpack certain expression results.
        //
        // * We change type specifiers in these cases:
        //   - struct members
        //   - pointer store types
        //   - variable declaration types
        //
        // * We convert a value that contains a regular vec3 to an equivalent value that uses the
        //   internal `__packed_vec3` in these cases:
        //   - `var` initializers
        //   - right-hand side of assignments
        //   - struct constructor arguments
        //
        // * We convert a value that will contain a `__packed_vec3` to an equivalent value that uses
        //   a regular vec3 in these cases:
        //   - loads from memory
        //   - member accesses into a value struct
        //
        // Pending pack and unpack operations are collected and elided if redundant, and applied
        // after the whole module has been processed.
        for (auto* node : ctx.src->ASTNodes().Objects()) {
            Switch(
                sem.Get(node),
                [&](const sem::TypeExpression* type) {
                    // Rewrite pointers to types that contain vec3s.
                    if (auto* ptr = type->Type()->As<type::Pointer>();
                        ptr && builtin::IsHostShareable(ptr->AddressSpace())) {
                        auto* store_type = ptr->StoreType();
                        ast::Type new_store_type{};
                        if (IsVec3(store_type)) {
                            new_store_type = MakePackedVec3(store_type);
                        } else {
                            new_store_type = RewriteArrayType(store_type);
                        }
                        if (new_store_type) {
                            auto access = ptr->AddressSpace() == builtin::AddressSpace::kStorage
                                              ? ptr->Access()
                                              : builtin::Access::kUndefined;
                            auto new_ptr_type =
                                b.ty.pointer(new_store_type, ptr->AddressSpace(), access);
                            ctx.Replace(node, new_ptr_type.expr);
                        }
                    }
                },
                [&](const sem::Variable* var) {
                    if (!builtin::IsHostShareable(var->AddressSpace())) {
                        return;
                    }

                    // Rewrite the var type, if it contains vec3s.
                    auto* store_type = var->Type()->UnwrapRef();
                    ast::Type new_store_type{};
                    if (IsVec3(store_type)) {
                        // TODO: consider unifying this as well
                        new_store_type = MakePackedVec3(store_type);
                    } else {
                        new_store_type = RewriteArrayType(store_type);
                    }
                    if (new_store_type) {
                        ctx.Replace(var->Declaration()->type.expr, new_store_type.expr);
                    }
                },
                [&](const sem::Statement* stmt) {
                    // Pack the RHS of assignment statements that are writing to packed types.
                    if (auto* assign = stmt->Declaration()->As<ast::AssignmentStatement>()) {
                        if (!builtin::IsHostShareable(sem.GetVal(assign->lhs)
                                                          ->Type()
                                                          ->As<type::Reference>()
                                                          ->AddressSpace())) {
                            // Skip assignments to address space that are not host-shareable.
                            return;
                        }
                        // TODO: just inline, only call now?
                        pack_if_needed(sem.GetVal(assign->rhs));
                    }
                },
                [&](const sem::Load* load) {
                    // Unpack loads of packed types.
                    if (ContainsVec3(load->Type()) &&
                        builtin::IsHostShareable(load->ReferenceType()->AddressSpace())) {
                        to_unpack.Add(load);
                    }
                },
                [&](const sem::StructMemberAccess* accessor) {
                    // TODO: comment
                    auto* ref = accessor->Object()->Type()->As<type::Reference>();
                    if (ref && builtin::IsHostShareable(ref->AddressSpace()) &&
                        IsVec3(accessor->Type()->UnwrapRef())) {
                        ctx.Replace(node, b.MemberAccessor(ctx.Clone(accessor->Declaration()),
                                                           kStructMemberName));
                    }
                },
                [&](const sem::IndexAccessorExpression* accessor) {
                    if (auto* ref = accessor->Type()->As<type::Reference>()) {
                        if (builtin::IsHostShareable(ref->AddressSpace())) {
                            // If we are extracting a reference to a vec3, extract the packed vector
                            // from the wrapper struct.
                            if (IsVec3(ref->StoreType())) {
                                ctx.Replace(node,
                                            b.MemberAccessor(ctx.Clone(accessor->Declaration()),
                                                             kStructMemberName));
                            }
                        }
                    }
                });
        }

        // Sort the pending pack/unpack operations by AST node ID to make the order deterministic.
        auto to_unpack_sorted = to_unpack.Vector();
        auto to_pack_sorted = to_pack.Vector();
        auto pred = [&](auto* expr_a, auto* expr_b) {
            return expr_a->Declaration()->node_id < expr_b->Declaration()->node_id;
        };
        to_unpack_sorted.Sort(pred);
        to_pack_sorted.Sort(pred);

        // Apply all of the pending unpack operations that we have collected.
        for (auto* expr : to_unpack_sorted) {
            TINT_ASSERT(Transform, ContainsVec3(expr->Type()));
            auto* packed = ctx.Clone(expr->Declaration());
            const ast::Expression* unpacked = nullptr;
            if (IsVec3(expr->Type())) {
                // TODO: wrap direct in struct too to unify this?
                if (expr->UnwrapLoad()
                        ->IsAnyOf<sem::IndexAccessorExpression, sem::StructMemberAccess>()) {
                    // If we are unpacking a vec3 that came from an array accessor, extract the
                    // vector from the wrapper struct.
                    packed = b.MemberAccessor(packed, kStructMemberName);
                }
                // Cast the packed vector to a regular vec3.
                unpacked = b.Call(CreateASTTypeFor(ctx, expr->Type()), packed);
            } else {
                // Use a helper function to unpack an array or matrix.
                unpacked = UnpackArray(packed, expr->Type());
            }
            TINT_ASSERT(Transform, unpacked != nullptr);
            ctx.Replace(expr->Declaration(), unpacked);
        }

        // Apply all of the pending pack operations that we have collected.
        for (auto* expr : to_pack_sorted) {
            TINT_ASSERT(Transform, ContainsVec3(expr->Type()));
            auto* unpacked = ctx.Clone(expr->Declaration());
            const ast::Expression* packed = nullptr;
            if (IsVec3(expr->Type())) {
                // Cast the regular vec3 to a packed vector type.
                // // TODO: can we just use PackArray (renamed) for this all the time now?
                // packed = b.Call(RewriteArrayType(expr->Type()),
                //                 b.Call(MakePackedVec3(expr->Type()), unpacked));
                packed = b.Call(MakePackedVec3(expr->Type()), unpacked);
            } else {
                // Use a helper function to pack an array or matrix.
                packed = PackArray(unpacked, expr->Type());
            }
            TINT_ASSERT(Transform, packed != nullptr);
            ctx.Replace(expr->Declaration(), packed);
        }

        ctx.Clone();
        return Program(std::move(b));
    }

  private:
    /// The source program
    const Program* const src;
    /// The target program builder
    ProgramBuilder b;
    /// The clone context
    CloneContext ctx = {&b, src, /* auto_clone_symbols */ true};
    /// Alias to the semantic info in ctx.src
    const sem::Info& sem = ctx.src->Sem();
};

PackedVec3::PackedVec3() = default;
PackedVec3::~PackedVec3() = default;

Transform::ApplyResult PackedVec3::Apply(const Program* src, const DataMap&, DataMap&) const {
    return State{src}.Run();
}

}  // namespace tint::transform
