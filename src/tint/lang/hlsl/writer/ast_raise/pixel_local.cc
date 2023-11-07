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

#include "src/tint/lang/hlsl/writer/ast_raise/pixel_local.h"

#include <utility>

#include "src/tint/lang/core/fluent_types.h"
#include "src/tint/lang/core/number.h"
#include "src/tint/lang/wgsl/program/clone_context.h"
#include "src/tint/lang/wgsl/resolver/resolve.h"
#include "src/tint/lang/wgsl/sem/function.h"
#include "src/tint/lang/wgsl/sem/module.h"
#include "src/tint/lang/wgsl/sem/statement.h"
#include "src/tint/lang/wgsl/sem/struct.h"
#include "src/tint/utils/containers/transform.h"

TINT_INSTANTIATE_TYPEINFO(tint::hlsl::writer::PixelLocal);
TINT_INSTANTIATE_TYPEINFO(tint::hlsl::writer::PixelLocal::RasterizerOrderedView);
TINT_INSTANTIATE_TYPEINFO(tint::hlsl::writer::PixelLocal::Config);

namespace tint::hlsl::writer {

struct PixelLocal::State {
    /// The source program
    const Program& src;
    /// The target program builder
    ProgramBuilder b;
    /// The clone context
    program::CloneContext ctx = {&b, &src, /* auto_clone_symbols */ true};
    /// The transform config
    const Config& cfg;

    /// Constructor
    /// @param program the source program
    /// @param config the transform config
    State(const Program& program, const Config& config) : src(program), cfg(config) {}

    /// Runs the transform
    /// @returns the new program or SkipTransform if the transform is not required
    ApplyResult Run() {
        auto& sem = src.Sem();

        // If the pixel local extension isn't enabled, then there must be no use of pixel_local
        // variables, and so there's nothing for this transform to do.
        if (!sem.Module()->Extensions().Contains(
                wgsl::Extension::kChromiumExperimentalPixelLocal)) {
            return SkipTransform;
        }

        bool made_changes = false;

        // Change all module scope `var<pixel_local>` variables to `var<private>`.
        // We need to do this even if the variable is not referenced by the entry point as later
        // stages do not understand the pixel_local address space.
        for (auto* global : src.AST().GlobalVariables()) {
            if (auto* var = global->As<ast::Var>()) {
                if (sem.Get(var)->AddressSpace() == core::AddressSpace::kPixelLocal) {
                    // Change the 'var<pixel_local>' to 'var<private>'
                    ctx.Replace(var->declared_address_space, b.Expr(core::AddressSpace::kPrivate));
                    made_changes = true;
                }
            }
        }

        // Find the single entry point
        const sem::Function* entry_point = nullptr;
        for (auto* fn : src.AST().Functions()) {
            if (fn->IsEntryPoint()) {
                if (entry_point != nullptr) {
                    TINT_ICE() << "PixelLocal transform requires that the SingleEntryPoint "
                                  "transform has already been run";
                    return SkipTransform;
                }
                entry_point = sem.Get(fn);

                // Look for a `var<pixel_local>` used by the entry point...
                const tint::sem::GlobalVariable* pixel_local_variable = nullptr;
                for (auto* global : entry_point->TransitivelyReferencedGlobals()) {
                    if (global->AddressSpace() == core::AddressSpace::kPixelLocal) {
                        pixel_local_variable = global;
                        made_changes = true;
                        break;
                    }
                }
                if (pixel_local_variable == nullptr) {
                    continue;
                }

                // Obtain struct of the pixel local.
                auto* pixel_local_str =
                    pixel_local_variable->Type()->UnwrapRef()->As<sem::Struct>();
                TransformEntryPoint(entry_point, pixel_local_variable, pixel_local_str);

                break;  // Only a single `var<pixel_local>` can be used by an entry point.
            }
        }

        if (!made_changes) {
            return SkipTransform;
        }

        ctx.Clone();
        return resolver::Resolve(b);
    }

    /// Transforms the entry point @p entry_point to handle the direct or transitive usage of the
    /// `var<pixel_local>` @p pixel_local_var.
    /// @param entry_point the entry point
    /// @param pixel_local_var the `var<pixel_local>`
    /// @param pixel_local_str the struct type of the var
    void TransformEntryPoint(const sem::Function* entry_point,
                             const sem::GlobalVariable* pixel_local_var,
                             const sem::Struct* pixel_local_str) {
        // Wrap the old entry point "fn" into a new entry point where functions to load and store
        // ROV data are called.
        auto* original_entry_point_fn = entry_point->Declaration();
        auto entry_point_name = original_entry_point_fn->name->symbol.Name();

        // Remove the @fragment attribute from the entry point
        ctx.Remove(original_entry_point_fn->attributes,
                   ast::GetAttribute<ast::StageAttribute>(original_entry_point_fn->attributes));
        // Rename the entry point.
        auto inner_function_name = b.Symbols().New(entry_point_name + "_inner");
        ctx.Replace(original_entry_point_fn->name, b.Ident(inner_function_name));

        // Create a new function that wraps the entry point.
        // This function has all the existing entry point parameters and an additional
        // parameter for the input pixel local structure.
        auto new_entry_point_params = ctx.Clone(original_entry_point_fn->params);

        // Remove any entry-point attributes from the inner function.
        // This must come after `ctx.Clone(fn->params)` as we want these attributes on the outer
        // function.
        for (auto* param : original_entry_point_fn->params) {
            for (auto* attr : param->attributes) {
                if (attr->IsAnyOf<ast::BuiltinAttribute, ast::LocationAttribute,
                                  ast::InterpolateAttribute, ast::InvariantAttribute>()) {
                    ctx.Remove(param->attributes, attr);
                }
            }
        }

        // Declare the ROVs for the members of the pixel local variable and the functions to
        // load data from and store data into the ROVs.
        auto load_rov_function_name = b.Symbols().New("load_from_pixel_local_storage");
        auto store_rov_function_name = b.Symbols().New("store_into_pixel_local_storage");
        DeclareROVsAndLoadStoreFunctions(
            load_rov_function_name.Name(), store_rov_function_name.Name(),
            pixel_local_var->Declaration()->name->symbol.Name(), pixel_local_str);

        // Declare new entry point
        Vector<const ast::Statement*, 5> new_entry_point_function_body;

        // 1. let hlsl_sv_position = @builtin(position)
        // Declare @builtin(position) in the input parameter of the new entry point if it is not
        // declared in the original entry point.
        auto sv_position_symbol = b.Symbols().New("hlsl_sv_position");
        new_entry_point_function_body.Push(DeclareVariableWithBuiltinPosition(
            &new_entry_point_params, sv_position_symbol, entry_point));

        // 2. Call `load_from_pixel_local_storage(hlsl_sv_position)`
        new_entry_point_function_body.Push(
            b.CallStmt(b.Call(load_rov_function_name.Name(), sv_position_symbol.Name())));

        // Declare the inner function
        // Build the arguments to call the inner function
        auto inner_function_call_args = tint::Transform(
            original_entry_point_fn->params, [&](auto* p) { return b.Expr(ctx.Clone(p->name)); });

        ast::Type new_entry_point_return_type;
        if (original_entry_point_fn->return_type) {
            // Create a structure to hold the combined flattened result of the entry point with
            // `@position` attribute
            auto entry_point_return_struct_name = b.Symbols().New(entry_point_name + "_res");
            Vector<const ast::StructMember*, 8> members;
            Vector<const ast::Expression*, 8>
                return_args;  // arguments to the final `return` statement

            auto add_member = [&](const core::type::Type* ty,
                                  VectorRef<const ast::Attribute*> attrs) {
                members.Push(b.Member("output_" + std::to_string(members.Length()),
                                      CreateASTTypeFor(ctx, ty), std::move(attrs)));
            };

            Symbol inner_function_call_result = b.Symbols().New("result");
            if (auto* str = entry_point->ReturnType()->As<sem::Struct>()) {
                // The entry point returned a structure.
                for (auto* member : str->Members()) {
                    auto& member_attrs = member->Declaration()->attributes;
                    add_member(member->Type(), ctx.Clone(member_attrs));
                    return_args.Push(
                        b.MemberAccessor(inner_function_call_result, ctx.Clone(member->Name())));
                    if (auto* location = ast::GetAttribute<ast::LocationAttribute>(member_attrs)) {
                        // Remove the @location attribute from the member of the inner function's
                        // output structure.
                        // Note: This will break other entry points that share the same output
                        // structure, however this transform assumes that the SingleEntryPoint
                        // transform will have already been run.
                        ctx.Remove(member_attrs, location);
                    }
                }
            } else {
                // The entry point returned a non-structure
                add_member(entry_point->ReturnType(),
                           ctx.Clone(original_entry_point_fn->return_type_attributes));
                return_args.Push(b.Expr(inner_function_call_result));

                // Remove the @location from the inner function's return type attributes
                ctx.Remove(original_entry_point_fn->return_type_attributes,
                           ast::GetAttribute<ast::LocationAttribute>(
                               original_entry_point_fn->return_type_attributes));
            }

            // 3. Call inner function and get the return value
            new_entry_point_function_body.Push(
                b.Decl(b.Let(inner_function_call_result,
                             b.Call(inner_function_name, std::move(inner_function_call_args)))));

            // Declare the output structure
            b.Structure(entry_point_return_struct_name, std::move(members));

            // 4. Call `store_into_pixel_local_storage(hlsl_sv_position)`
            new_entry_point_function_body.Push(
                b.CallStmt(b.Call(store_rov_function_name.Name(), sv_position_symbol.Name())));

            // 5. Return the output structure
            new_entry_point_function_body.Push(
                b.Return(b.Call(entry_point_return_struct_name, std::move(return_args))));

            new_entry_point_return_type = b.ty(entry_point_return_struct_name);
        } else {
            // 3. Call inner function without return value
            new_entry_point_function_body.Push(
                b.CallStmt(b.Call(inner_function_name, std::move(inner_function_call_args))));

            // 4. Call `store_into_pixel_local_storage(hlsl_sv_position)`
            new_entry_point_function_body.Push(
                b.CallStmt(b.Call(store_rov_function_name.Name(), sv_position_symbol.Name())));

            new_entry_point_return_type = b.ty.void_();
        }

        // Declare the new entry point that calls the inner function
        b.Func(entry_point_name, std::move(new_entry_point_params), new_entry_point_return_type,
               new_entry_point_function_body, Vector{b.Stage(ast::PipelineStage::kFragment)});
    }

    void DeclareROVsAndLoadStoreFunctions(const std::string& load_rov_function_name,
                                          const std::string& store_rov_function_name,
                                          const std::string& pixel_local_variable_name,
                                          const sem::Struct* pixel_local_str) {
        std::string load_store_input_name = "my_input";
        Vector<const ast::Parameter*, 1> load_parameters(
            {b.Param(load_store_input_name, b.ty.vec4<core::f32>())});
        Vector<const ast::Parameter*, 1> store_parameters(
            {b.Param(load_store_input_name, b.ty.vec4<core::f32>())});

        Vector<const ast::Statement*, 8> load_body;
        Vector<const ast::Statement*, 8> store_body;
        for (auto* member : pixel_local_str->Members()) {
            // Declare the read-write storage texture with RasterizerOrderedView attribute.
            core::TexelFormat rovFormat = core::TexelFormat::kUndefined;
            std::string pixelType;
            if (member->Type()->Is<core::type::F32>()) {
                rovFormat = core::TexelFormat::kR32Float;
                pixelType = "vec4f";
            } else if (member->Type()->Is<core::type::I32>()) {
                rovFormat = core::TexelFormat::kR32Sint;
                pixelType = "vec4i";
            } else if (member->Type()->Is<core::type::U32>()) {
                rovFormat = core::TexelFormat::kR32Uint;
                pixelType = "vec4u";
            }
            auto rovType = b.ty.storage_texture(core::type::TextureDimension::k2d, rovFormat,
                                                core::Access::kReadWrite);
            auto rovSymbolName = b.Symbols().New("pixel_local_" + member->Name().Name());
            tint::Vector<const ast::Attribute*, 3> attributes;
            attributes.Push(b.Binding(core::AInt(RegisterIndex(member->Index()))));
            attributes.Push(b.Group(core::AInt(cfg.rovGroupIndex)));
            attributes.Push(RasterizerOrderedView());
            b.GlobalVar(rovSymbolName.Name(), rovType, attributes);

            // The function body of loading from PLS
            // let rov_texcoord_i = vec2u(my_input.xy);
            // PLS_Private_Variable.member = textureLoad(pixel_local_member, rov_texcoord_i).x;
            std::string rov_texcoord = "rov_texcoord_" + std::to_string(member->Index());
            load_body.Push(b.Decl(b.Let(
                rov_texcoord, b.Call("vec2u", b.MemberAccessor(load_store_input_name, "xy")))));
            auto pixel_local_var_member_access1 =
                b.MemberAccessor(pixel_local_variable_name, member->Name().Name());
            auto load_call = b.Call(wgsl::BuiltinFn::kTextureLoad, rovSymbolName, rov_texcoord);
            auto to_scalar_call = b.MemberAccessor(load_call, "x");
            load_body.Push(b.Assign(pixel_local_var_member_access1, to_scalar_call));

            // The function body of storing data into PLS
            // let rov_texcoord_i = vec2u(my_input.xy);
            // textureStore(pixel_local_member, rov_texcoord_i, vec4u(PLS_Private_Variable.member));
            store_body.Push(b.Decl(b.Let(
                rov_texcoord, b.Call("vec2u", b.MemberAccessor(load_store_input_name, "xy")))));
            auto pixel_local_var_member_access2 =
                b.MemberAccessor(pixel_local_variable_name, member->Name().Name());
            auto to_vec4_call = b.Call(pixelType, pixel_local_var_member_access2);
            auto store_call = b.Call(wgsl::BuiltinFn::kTextureStore, rovSymbolName.Name(),
                                     rov_texcoord, to_vec4_call);
            store_body.Push(b.CallStmt(store_call));
        }

        b.Func(load_rov_function_name, std::move(load_parameters), b.ty.void_(), load_body);
        b.Func(store_rov_function_name, std::move(store_parameters), b.ty.void_(), store_body);
    }

    // Find and get @builtin(position) which is needed for loading and storing data with ROVs
    const ast::VariableDeclStatement* DeclareVariableWithBuiltinPosition(
        tint::Vector<const ast::Parameter*, 8>* new_entry_point_params,
        const tint::Symbol& variable_with_position_symbol,
        const sem::Function* entry_point) {
        auto& sem = src.Sem();

        for (size_t i = 0; i < entry_point->Parameters().Length(); ++i) {
            auto* parameter = entry_point->Parameters()[i];
            // 1. @builtin(position) is declared as a member of a structure
            if (auto* struct_type = parameter->Type()->As<sem::Struct>()) {
                for (size_t memberIndex = 0; memberIndex < struct_type->Members().Length();
                     ++memberIndex) {
                    auto* member = struct_type->Members()[memberIndex];
                    if (auto* attribute = ast::GetAttribute<ast::BuiltinAttribute>(
                            member->Declaration()->attributes)) {
                        auto builtin = sem.Get(attribute)->Value();
                        if (builtin == core::BuiltinValue::kPosition) {
                            return b.Decl(b.Let(variable_with_position_symbol,
                                                b.MemberAccessor((*new_entry_point_params)[i],
                                                                 member->Name().Name())));
                        }
                    }
                }
            }
            // 2. @builtin(position) is declared as an individual input parameter
            if (auto* attribute = ast::GetAttribute<ast::BuiltinAttribute>(
                    parameter->Declaration()->attributes)) {
                auto builtin = sem.Get(attribute)->Value();
                if (builtin == core::BuiltinValue::kPosition) {
                    return b.Decl(
                        b.Let(variable_with_position_symbol, b.Expr((*new_entry_point_params)[i])));
                }
            }
        }

        // 3. @builtin(position) is not declared in the input parameters and we should add one
        auto* new_position = b.Param(b.Symbols().New("my_pos"), b.ty.vec4<core::f32>(),
                                     Vector{b.Builtin(core::BuiltinValue::kPosition)});
        new_entry_point_params->Push(new_position);
        return b.Decl(b.Let(variable_with_position_symbol, b.Expr(new_position)));
    }

    /// @returns a new RasterizerOrderedView attribute
    PixelLocal::RasterizerOrderedView* RasterizerOrderedView() {
        return b.ASTNodes().Create<PixelLocal::RasterizerOrderedView>(b.ID(), b.AllocateNodeID());
    }

    /// @returns the register index for the pixel local field with the given index
    /// @param field_index the pixel local field index
    uint32_t RegisterIndex(uint32_t field_index) {
        auto idx = cfg.pixelLocalStructureMemberIndexToROVRegister.Get(field_index);
        if (TINT_UNLIKELY(!idx)) {
            b.Diagnostics().add_error(diag::System::Transform,
                                      "PixelLocal::Config::attachments missing entry for field " +
                                          std::to_string(field_index));
            return 0;
        }
        return *idx;
    }
};

PixelLocal::PixelLocal() = default;

PixelLocal::~PixelLocal() = default;

ast::transform::Transform::ApplyResult PixelLocal::Apply(const Program& src,
                                                         const ast::transform::DataMap& inputs,
                                                         ast::transform::DataMap&) const {
    auto* cfg = inputs.Get<Config>();
    if (!cfg) {
        ProgramBuilder b;
        b.Diagnostics().add_error(diag::System::Transform,
                                  "missing transform data for " + std::string(TypeInfo().name));
        return resolver::Resolve(b);
    }

    return State(src, *cfg).Run();
}

PixelLocal::Config::Config() = default;

PixelLocal::Config::Config(const Config&) = default;

PixelLocal::Config::~Config() = default;

PixelLocal::RasterizerOrderedView::RasterizerOrderedView(GenerationID pid, ast::NodeID nid)
    : Base(pid, nid, Empty) {}

PixelLocal::RasterizerOrderedView::~RasterizerOrderedView() = default;

std::string PixelLocal::RasterizerOrderedView::InternalName() const {
    return "rov";
}

const PixelLocal::RasterizerOrderedView* PixelLocal::RasterizerOrderedView::Clone(
    ast::CloneContext& ctx) const {
    return ctx.dst->ASTNodes().Create<RasterizerOrderedView>(ctx.dst->ID(),
                                                             ctx.dst->AllocateNodeID());
}

}  // namespace tint::hlsl::writer
