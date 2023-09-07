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

#include "src/tint/lang/msl/writer/ast_raise/pixel_local.h"

#include <utility>

#include "src/tint/lang/core/fluent_types.h"
#include "src/tint/lang/wgsl/program/clone_context.h"
#include "src/tint/lang/wgsl/resolver/resolve.h"
#include "src/tint/lang/wgsl/sem/function.h"
#include "src/tint/lang/wgsl/sem/module.h"
#include "src/tint/lang/wgsl/sem/statement.h"
#include "src/tint/lang/wgsl/sem/struct.h"
#include "src/tint/utils/containers/transform.h"

TINT_INSTANTIATE_TYPEINFO(tint::msl::writer::PixelLocal);
TINT_INSTANTIATE_TYPEINFO(tint::msl::writer::PixelLocal::Attachment);
TINT_INSTANTIATE_TYPEINFO(tint::msl::writer::PixelLocal::Config);

using namespace tint::core::number_suffixes;  // NOLINT
using namespace tint::core::fluent_types;     // NOLINT

namespace tint::msl::writer {

/// PIMPL state for the transform
struct PixelLocal::State {
    /// The source program
    const Program* const src;
    /// The target program builder
    ProgramBuilder b;
    /// The clone context
    program::CloneContext ctx = {&b, src, /* auto_clone_symbols */ true};
    /// The transform config
    const Config& cfg;

    /// Constructor
    /// @param program the source program
    /// @param config the transform config
    State(const Program* program, const Config& config) : src(program), cfg(config) {}

    /// Runs the transform
    /// @returns the new program or SkipTransform if the transform is not required
    ApplyResult Run() {
        auto& sem = src->Sem();
        if (!sem.Module()->Extensions().Contains(
                core::Extension::kChromiumExperimentalPixelLocal)) {
            return SkipTransform;
        }

        Vector<const sem::Function*, 8> entry_points;
        for (auto* fn : src->AST().Functions()) {
            if (fn->IsEntryPoint()) {
                entry_points.Push(sem.Get(fn));
            }
        }

        bool made_changes = false;

        // Collect the module-scope 'var<pixel_local>' declarations, replace them with
        // 'var<private>'.
        for (auto* global : src->AST().GlobalVariables()) {
            auto* pixel_local_var = global->As<ast::Var>();
            if (!pixel_local_var) {
                continue;
            }
            auto* v = sem.Get<sem::GlobalVariable>(global);
            if (v->AddressSpace() != core::AddressSpace::kPixelLocal) {
                continue;
            }

            auto pixel_local_var_name = ctx.Clone(pixel_local_var->name->symbol);

            ctx.Replace(pixel_local_var->declared_address_space,
                        b.Expr(core::AddressSpace::kPrivate));
            made_changes = true;

            // Examine the type of the pixel_local variable.
            auto* pixel_local_str = v->Type()->UnwrapRef()->As<sem::Struct>();
            auto pixel_local_str_name = ctx.Clone(pixel_local_str->Name());

            // Add an attachment decoration to each member of the pixel_local structure.
            for (auto* member : pixel_local_str->Members()) {
                ctx.InsertBack(member->Declaration()->attributes,
                               Attachment(AttachmentIndex(member->Index())));
                ctx.InsertBack(member->Declaration()->attributes,
                               b.Disable(ast::DisabledValidation::kEntryPointParameter));
            }

            // For each entry point that uses this pixel local var...
            for (auto* ep : entry_points) {
                if (!ep->TransitivelyReferencedGlobals().Contains(v)) {
                    continue;
                }

                auto* fn = ep->Declaration();
                auto fn_name = fn->name->symbol.Name();

                // Remove the @fragment attribute from the entry point
                ctx.Remove(fn->attributes, ast::GetAttribute<ast::StageAttribute>(fn->attributes));
                // Rename the entry point
                auto inner_name = b.Symbols().New(fn_name + "_inner");
                ctx.Replace(fn->name, b.Ident(inner_name));

                // Create a new function that wraps the entry point.
                // This function has all the existing entry point parameters and an additional
                // parameter for the input pixel local structure.
                auto params = ctx.Clone(fn->params);
                auto pl_param = b.Symbols().New("pixel_local");
                params.Push(b.Param(pl_param, b.ty(pixel_local_str_name)));

                // Build the outer function's statements
                // Begin the outer function with a copy of the pixel local parameter to the pixel
                // local var
                Vector<const ast::Statement*, 3> body{
                    b.Assign(pixel_local_var_name, pl_param),
                };

                // Build the arguments to call the inner function
                auto call_args =
                    tint::Transform(fn->params, [&](auto* p) { return b.Expr(p->name->symbol); });

                // Create a structure to hold the combined flattened result of the entry point
                // and the pixel local structure.
                auto str_name = b.Symbols().New(fn_name + "_res");

                Symbol call_result;

                Vector<const ast::StructMember*, 8> members;
                Vector<const ast::Expression*, 8> return_args;
                auto add_member = [&](const core::type::Type* ty,
                                      VectorRef<const ast::Attribute*> attrs) {
                    members.Push(b.Member("output_" + std::to_string(members.Length()),
                                          CreateASTTypeFor(ctx, ty), std::move(attrs)));
                };
                for (auto* member : pixel_local_str->Members()) {
                    add_member(member->Type(),
                               Vector{
                                   b.Location(AInt(AttachmentIndex(member->Index()))),
                                   b.Disable(ast::DisabledValidation::kEntryPointParameter),
                               });
                    return_args.Push(
                        b.MemberAccessor(pixel_local_var_name, ctx.Clone(member->Name())));
                }
                if (fn->return_type) {
                    call_result = b.Symbols().New("result");
                    if (auto* str = ep->ReturnType()->As<sem::Struct>()) {
                        for (auto* member : str->Members()) {
                            auto& member_attrs = member->Declaration()->attributes;
                            add_member(member->Type(), ctx.Clone(member_attrs));
                            return_args.Push(
                                b.MemberAccessor(call_result, ctx.Clone(member->Name())));
                            if (auto* location =
                                    ast::GetAttribute<ast::LocationAttribute>(member_attrs)) {
                                // Remove the @location attribute from the inner function's output
                                // structure. The writer doesn't like having non-entrypoint
                                // structures annotated with these attributes.
                                ctx.Remove(member_attrs, location);
                            }
                        }
                    } else {
                        add_member(ep->ReturnType(), ctx.Clone(fn->return_type_attributes));
                        return_args.Push(b.Expr(call_result));
                    }
                    body.Push(b.Decl(b.Let(call_result, b.Call(inner_name, std::move(call_args)))));
                } else {
                    body.Push(b.CallStmt(b.Call(inner_name, std::move(call_args))));
                }

                b.Structure(str_name, std::move(members));
                body.Push(b.Return(b.Call(str_name, std::move(return_args))));
                auto ret_ty = b.ty(str_name);

                Vector attrs{b.Stage(ast::PipelineStage::kFragment)};

                b.Func(fn_name, std::move(params), ret_ty, body, attrs);
            }
        }

        if (!made_changes) {
            return SkipTransform;
        }

        ctx.Clone();
        return resolver::Resolve(b);
    }

    /// @returns a new Attachment attribute
    /// @param index the index of the attachment
    PixelLocal::Attachment* Attachment(uint32_t index) {
        return b.ASTNodes().Create<PixelLocal::Attachment>(b.ID(), b.AllocateNodeID(), index);
    }

    /// @returns the attachment index for the pixel local field with the given index
    /// @param field_index the pixel local field index
    uint32_t AttachmentIndex(uint32_t field_index) {
        auto idx = cfg.attachments.Get(field_index);
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

ast::transform::Transform::ApplyResult PixelLocal::Apply(const Program* src,
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

PixelLocal::Attachment::Attachment(GenerationID pid, ast::NodeID nid, uint32_t idx)
    : Base(pid, nid, Empty), index(idx) {}

PixelLocal::Attachment::~Attachment() = default;

std::string PixelLocal::Attachment::InternalName() const {
    return "attachment(" + std::to_string(index) + ")";
}

const PixelLocal::Attachment* PixelLocal::Attachment::Clone(ast::CloneContext& ctx) const {
    return ctx.dst->ASTNodes().Create<Attachment>(ctx.dst->ID(), ctx.dst->AllocateNodeID(), index);
}

}  // namespace tint::msl::writer
