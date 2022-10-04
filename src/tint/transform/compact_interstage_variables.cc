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

#include "src/tint/transform/compact_interstage_variables.h"

#include <memory>
#include <unordered_set>
#include <utility>

#include "src/tint/program_builder.h"
#include "src/tint/sem/call.h"
#include "src/tint/sem/function.h"
#include "src/tint/sem/member_accessor_expression.h"
#include "src/tint/sem/variable.h"
#include "src/tint/text/unicode.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::CompactInterstageVariables);
TINT_INSTANTIATE_TYPEINFO(tint::transform::CompactInterstageVariables::Config);

namespace tint::transform {

namespace {

}  // namespace

// CompactInterstageVariables::Data::Data(Remappings&& r) : remappings(std::move(r)) {}
// CompactInterstageVariables::Data::Data(const Data&) = default;
// CompactInterstageVariables::Data::~Data() = default;

// CompactInterstageVariables::Config::Config(Target t, bool pu) : target(t), preserve_unicode(pu)
// {} CompactInterstageVariables::Config::Config(const Config&) = default;
// CompactInterstageVariables::Config::~Config() = default;

CompactInterstageVariables::CompactInterstageVariables() = default;
CompactInterstageVariables::~CompactInterstageVariables() = default;

bool CompactInterstageVariables::ShouldRun(const Program* program, const DataMap& data) const {
    for (auto* func_ast : program->AST().Functions()) {
        if (!func_ast->IsEntryPoint()) {
            continue;
        }

        // if (func_ast->PipelineStage() == ast::PipelineStage::kFragment) {
        if (func_ast->PipelineStage() == ast::PipelineStage::kVertex) {
            if (auto* cfg = data.Get<Config>()) {
                // if (!cfg->interstage_variables.empty()) {
                //     return true;
                // }

                // if (!cfg->interstage_locations.IsEmpty()) {
                //     return true;
                // }

                // temp
                return true;
            }
        }
    }
    return false;
}

void CompactInterstageVariables::Run(CloneContext& ctx,
                                     const DataMap& config,
                                     DataMap& outputs) const {
    const auto* data = config.Get<Config>();
    if (!data) {
        ctx.dst->Diagnostics().add_error(diag::System::Transform,
                                         "Missing interstage input variable data");
        return;
    }

    auto& b = *ctx.dst;
    auto& sem = ctx.src->Sem();
    auto& sym = ctx.src->Symbols();

    // for (const auto& v : data->interstage_variables) {
    //     truncated_members.Push(b.Member(b.Sym().to_str(), b.ty.vec4(b.ty.f32())));
    // }

    // ctx.ReplaceAll([&](const ast::Function* func_ast){
    //     if (!func_ast->IsEntryPoint()) {
    //         return ctx.CloneWithoutTransform(func_ast);
    //     }
    //     if (func_ast->PipelineStage() != ast::PipelineStage::kVertex) {
    //          // Currently only vertex stage could have interstage output variables.
    //         return ctx.CloneWithoutTransform(func_ast);
    //     }


    //     if (auto* str = sem.Get(func_ast)->ReturnType()->As<sem::Struct>()) {
    //         const ast::Struct* struct_ty = str->Declaration();

    //         utils::Vector<const ast::StructMember*, 16> truncated_members;
    //         utils::Vector<const ast::StructMember*, 4> builtin_output_members;
    //         utils::Vector<const ast::Statement*, 20> truncate_func_statements {
    //             b.Decl(b.Var("result", b.ty.type_name("TruncatedShaderIO")))
    //         };

    //         // Get a new struct?
    //         for (auto* member : struct_ty->members) {
    //             if (ast::GetAttribute<ast::BuiltinAttribute>(member->attributes)) {
    //                 // builtin_output_members.Push(member);
    //                 auto* m = sem.Get(member);
    //                 builtin_output_members.Push(b.Member(m->Name().to_str(), member->type, member->attributes));
    //                 truncate_func_statements.Push(
    //                     b.Assign(
    //                         b.MemberAccessor("result", sem.Get(member)->Name().to_str()),
    //                         b.MemberAccessor("io", sem.Get(member)->Name().to_str())
    //                     )
    //                 );
    //                 continue;
    //             }

    //             if (auto* attr = ast::GetAttribute<ast::LocationAttribute>(member->attributes)) {
    //                 // uint32_t location = 3;
    //                 auto* m = sem.Get(member);
    //                 uint32_t location = m->Location().value();

    //                 // if (data->interstage_locations[static_cast<size_t>(location)]) {
    //                 if (data->interstage_locations[location] == true) {
    //                     // truncated_members.Push(member);
                        
    //                     truncated_members.Push(b.Member(m->Name().to_str(), member->type, member->attributes));

    //                     truncate_func_statements.Push(
    //                         b.Assign(
    //                             b.MemberAccessor("result", sem.Get(member)->Name().to_str()),
    //                             b.MemberAccessor("io", sem.Get(member)->Name().to_str())
    //                         )
    //                     );
    //                 }
    //             }
    //         }

    //         truncate_func_statements.Push(
    //             b.Return("result")
    //         );

    //         if (!builtin_output_members.IsEmpty()) {
    //             // temp, use vector range (slice?) copy
    //             for (const ast::StructMember* builtin_member : builtin_output_members) {
    //                 truncated_members.Push(builtin_member);
    //             }
    //         }
    //         auto* new_structure = b.Structure(b.Symbols().New("TruncatedShaderIO"), truncated_members);
    //         // auto* new_structure = b.Structure(b.Symbols().New("TruncatedShaderIO"), utils::Empty);

    //         auto mapping_fn_sym = b.Symbols().New("truncate_shader_output");
    //         auto* mapping_fn = b.Func(
    //             mapping_fn_sym,
    //             utils::Vector{
    //                 b.Param("io", func_ast->return_type)
    //                 // b.Param("io", sem.Get(func_ast)->ReturnType()->)
    //             },
    //             b.ty.type_name("TruncatedShaderIO"),
    //             truncate_func_statements
    //         );

    //         return mapping_fn;
    //     }

    //     return ctx.CloneWithoutTransform(func_ast);
    // });


    for (auto* func_ast : ctx.src->AST().Functions()) {
        if (!func_ast->IsEntryPoint()) {
            continue;
        }

        if (func_ast->PipelineStage() != ast::PipelineStage::kVertex) {
            // Currently only fragment stage could have interstage input variables.
            continue;
        }

        utils::Vector<const ast::StructMember*, 16> truncated_members;
        utils::Vector<const ast::StructMember*, 4> builtin_output_members;

        auto* str = sem.Get(func_ast)->ReturnType()->As<sem::Struct>();
        // auto* return_type = sem.Get(func_ast)->ReturnType();
        // auto* str = return_type->As<sem::Struct>();
        if (str) {
            // This transform is run after CanonicalizeEntryPointIO transform,
            // So it is guaranteed that entry point input already grouped in a struct.
            const ast::Struct* struct_ty = str->Declaration();

            // utils::Vector<const ast::StructMember*, 8> members;

            // utils::Vector<const ast::Statement*, 20> truncate_func_statements;

            auto new_struct_sym = b.Symbols().New("TruncatedShaderIO");
            // new_struct_sym.to_str()

            // ctx.Replace(struct_ty, b.ty.type_name("TruncatedShaderIO"));

            utils::Vector<const ast::Statement*, 20> truncate_func_statements {
                b.Decl(b.Var("result", b.ty.type_name("TruncatedShaderIO")))
            };

            // Get a new struct?
            for (auto* member : struct_ty->members) {
                if (ast::GetAttribute<ast::BuiltinAttribute>(member->attributes)) {
                    // builtin_output_members.Push(member);
                    auto* m = sem.Get(member);
                    builtin_output_members.Push(b.Member(m->Name().to_str(), member->type, member->attributes));
                    truncate_func_statements.Push(
                        b.Assign(
                            b.MemberAccessor("result", sem.Get(member)->Name().to_str()),
                            b.MemberAccessor("io", sem.Get(member)->Name().to_str())
                        )
                    );
                    continue;
                }

                if (auto* attr = ast::GetAttribute<ast::LocationAttribute>(member->attributes)) {
                    // auto* materialize = Materialize(Expression(attr->expr));
                    // if (!materialize) {
                    //     continue;
                    // }
                    // auto* c = materialize->ConstantValue();
                    // if (!c) {
                    //     // TODO(crbug.com/tint/1633): Add error message about invalid materialization
                    //     // when location can be an expression.
                    //     continue;
                    // }
                    // uint32_t location = c->As<uint32_t>();

                    // uint32_t location = attr->expr
                    
                    // uint32_t location = 3;

                    auto* m = sem.Get(member);
                    uint32_t location = m->Location().value();

                    // if (data->interstage_locations[static_cast<size_t>(location)]) {
                    if (data->interstage_locations[location] == true) {
                        // truncated_members.Push(member);
                        
                        truncated_members.Push(b.Member(m->Name().to_str(), member->type, member->attributes));

                        truncate_func_statements.Push(
                            b.Assign(
                                b.MemberAccessor("result", sem.Get(member)->Name().to_str()),
                                b.MemberAccessor("io", sem.Get(member)->Name().to_str())
                            )
                        );
                    }
                }

                // LocationAttribute compare with interstage info
            }

            truncate_func_statements.Push(
                b.Return("result")
            );

            // auto* new_in_struct =
            //     ctx.dst->create<ast::Struct>(str->Name(), members, utils::Empty);

            // // ctx.Remove(func_ast->params, struct_ty);
            // // ctx.InsertBefore(ctx.src->AST().GlobalDeclarations(), func_ast, new_in_struct);
            // ctx.Replace(struct_ty, new_in_struct);


            if (!builtin_output_members.IsEmpty()) {
                // temp, use vector range (slice?) copy
                for (const ast::StructMember* builtin_member : builtin_output_members) {
                    truncated_members.Push(builtin_member);
                }
            }
            auto* new_structure = b.Structure(new_struct_sym, truncated_members);
            // auto* new_structure = b.Structure(b.Symbols().New("TruncatedShaderIO"), utils::Empty);

            auto mapping_fn_sym = b.Symbols().New("truncate_shader_output");
            auto* mapping_fn = b.Func(
                mapping_fn_sym,
                utils::Vector{
                    b.Param("io", CreateASTTypeFor(ctx, str))   // ?
                    // b.Param("io", sem.Get(struct_ty))
                    // b.Param("io", func_ast->return_type)
                    
                    // b.Param("io", sem.Get(func_ast)->ReturnType()->)
                },
                b.ty.type_name("TruncatedShaderIO"),
                truncate_func_statements
            );

            // ctx.InsertBefore(ctx.src->AST().GlobalDeclarations(), func_ast, mapping_fn);
            // ctx.InsertBefore(ctx.src->AST().GlobalDeclarations(), mapping_fn, new_structure);
            // ctx.InsertBefore(ctx.src->AST().GlobalDeclarations(), func_ast, mapping_fn).InsertBefore(ctx.src->AST().GlobalDeclarations(), mapping_fn, new_structure);

            ctx.ReplaceAll([&](const ast::ReturnStatement* return_statement) {
                // TODO: if return statement belongs to the entry point function

                return b.Return(return_statement->source, b.Call(mapping_fn_sym, ctx.Clone(return_statement->value)));
            });

        } else {
            TINT_UNREACHABLE(Transform, ctx.dst->Diagnostics());
        }
        // if (ast::HasAttribute<ast::LocationAttribute>(param->attributes)) {
        //     auto* sem = ctx.src->Sem().Get<sem::Parameter>(param);
        //     printf("!!! %u \n", sem->Location().value());
        // }

        // Pad fragment input with any missing vertex output interstage vars
    }

    
    

    ctx.Clone();
}

CompactInterstageVariables::Config::Config() = default;

CompactInterstageVariables::Config::Config(const Config&) = default;

CompactInterstageVariables::Config::~Config() = default;

CompactInterstageVariables::Config& CompactInterstageVariables::Config::operator=(const Config&) =
    default;

}  // namespace tint::transform
