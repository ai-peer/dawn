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

// TODO
bool ShouldRun(const Program* program, const DataMap& data) {
    auto* cfg = data.Get<CompactInterstageVariables::Config>();
    if (!cfg) {
        return false;
    }

    for (auto* func_ast : program->AST().Functions()) {
        if (!func_ast->IsEntryPoint()) {
            continue;
        }

        // if (func_ast->PipelineStage() == ast::PipelineStage::kFragment) {
        if (func_ast->PipelineStage() == ast::PipelineStage::kVertex) {
            return true;
        }
    }
    return false;
}

}  // anonymous namespace

// CompactInterstageVariables::Data::Data(Remappings&& r) : remappings(std::move(r)) {}
// CompactInterstageVariables::Data::Data(const Data&) = default;
// CompactInterstageVariables::Data::~Data() = default;

// CompactInterstageVariables::Config::Config(Target t, bool pu) : target(t), preserve_unicode(pu)
// {} CompactInterstageVariables::Config::Config(const Config&) = default;
// CompactInterstageVariables::Config::~Config() = default;

CompactInterstageVariables::CompactInterstageVariables() = default;
CompactInterstageVariables::~CompactInterstageVariables() = default;

// void CompactInterstageVariables::Run(CloneContext& ctx,
//                                      const DataMap& config,
//                                      DataMap& outputs) const {
Transform::ApplyResult CompactInterstageVariables::Apply(const Program* src,
                                                         const DataMap& config,
                                                         DataMap& outputs) const {
    ProgramBuilder b;
    CloneContext ctx{&b, src, /* auto_clone_symbols */ true};

    if (!ShouldRun(src, config)) {
        return SkipTransform;
    }

    const auto* data = config.Get<Config>();

    // auto& b = *ctx.dst;
    auto& sem = ctx.src->Sem();
    auto& sym = ctx.src->Symbols();

    for (auto* func_ast : ctx.src->AST().Functions()) {
        if (!func_ast->IsEntryPoint()) {
            continue;
        }

        if (func_ast->PipelineStage() != ast::PipelineStage::kVertex) {
            // Currently only fragment stage could have interstage input variables.
            continue;
        }

        utils::Vector<const ast::StructMember*, 20> truncated_members;
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

            // auto* new_struct_ty = b.ty.type_name(new_struct_sym);

            // new_struct_sym.to_str()

            // ctx.Replace(struct_ty, b.ty.type_name("TruncatedShaderIO"));

            utils::Vector<const ast::Statement*, 22> truncate_func_statements{
                // b.Decl(b.Var("result", b.ty.type_name("TruncatedShaderIO")))
                b.Decl(b.Var("result", b.ty.type_name(new_struct_sym)))};

            // Get a new struct?
            for (auto* member : struct_ty->members) {
                if (auto* attr = ast::GetAttribute<ast::BuiltinAttribute>(member->attributes)) {
                    // builtin_output_members.Push(member);

                    std::string member_name = sym.NameFor(member->symbol);
                    builtin_output_members.Push(
                        // b.Member(member_name, member->type, member->attributes)
                        // b.Member(member_name, ctx.Clone(member->type), member->attributes)
                        b.Member(member_name, ctx.Clone(member->type),
                                 ctx.Clone(member->attributes)));
                    // truncate_func_statements.Push(b.Assign(b.MemberAccessor("result",
                    // member_name),
                    //                                        b.MemberAccessor("io", member_name)));
                    truncate_func_statements.Push(
                        b.Assign(b.MemberAccessor("result", ctx.Clone(member->symbol)),
                                 b.MemberAccessor("io", ctx.Clone(member->symbol))));

                    ctx.Remove(member->attributes, attr);

                    // continue;
                } else if (auto* attr =
                               ast::GetAttribute<ast::LocationAttribute>(member->attributes)) {
                    auto* m = sem.Get(member);
                    uint32_t location = m->Location().value();
                    if (data->interstage_locations[location] == true) {
                        // truncated_members.Push(member);

                        std::string member_name = sym.NameFor(member->symbol);
                        truncated_members.Push(b.Member(member_name, ctx.Clone(member->type),
                                                        ctx.Clone(member->attributes)));
                        // b.Member(member_name, ctx.Clone(member->type), member->attributes));

                        truncate_func_statements.Push(
                            b.Assign(b.MemberAccessor("result", ctx.Clone(member->symbol)),
                                     b.MemberAccessor("io", ctx.Clone(member->symbol))));
                    }
                    ctx.Remove(member->attributes, attr);
                }
            }

            truncate_func_statements.Push(b.Return("result"));

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

            // auto* new_structure = b.Structure(ctx.Clone(new_struct_sym), truncated_members);
            auto* new_structure = b.Structure(new_struct_sym, truncated_members);
            // auto* new_structure = b.Structure(new_struct_sym, utils::Empty);

            auto mapping_fn_sym = b.Symbols().New("truncate_shader_output");
            auto* mapping_fn =
                b.Func(mapping_fn_sym,
                       utils::Vector{
                           // b.Param("io", CreateASTTypeFor(ctx, str))   // ?

                           // b.Param("io", sem.Get(struct_ty))
                           //    b.Param("io", ctx.Clone(func_ast->return_type))
                           b.Param("io", ctx.Clone(func_ast->return_type))
                           // b.Param("io", func_ast->return_type)
                           // b.Param("io", sem.Get(func_ast)->ReturnType())
                           // b.Param("io", b.ty.type_name(sem.Get(func_ast)->ReturnType()))

                           // b.Param("io", sem.Get(func_ast)->ReturnType()->)
                       },
                       // b.ty.type_name("TruncatedShaderIO"),
                       b.ty.type_name(new_struct_sym),
                       // truncate_func_statements
                       truncate_func_statements
                       // utils::Empty
                       // utils::Vector{
                       //     b.Decl(b.Var("result", b.ty.type_name("TruncatedShaderIO"))),
                       //     b.Return("result")
                       // }
                );

            ctx.Replace(func_ast->return_type, b.ty.type_name(new_struct_sym));

            // ctx.InsertBefore(ctx.src->AST().GlobalDeclarations(), func_ast, mapping_fn);
            // ctx.InsertBefore(ctx.src->AST().GlobalDeclarations(), mapping_fn, new_structure);
            // ctx.InsertBefore(ctx.src->AST().GlobalDeclarations(), func_ast,
            // mapping_fn).InsertBefore(ctx.src->AST().GlobalDeclarations(), mapping_fn,
            // new_structure);

            ctx.ReplaceAll([&](const ast::ReturnStatement* return_statement) {
                // TODO: if return statement belongs to the entry point function
                // Symbol is Valid

                return b.Return(return_statement->source,
                                b.Call(mapping_fn_sym, ctx.Clone(return_statement->value)));
                // b.Call(mapping_fn_sym, ctx.Clone(return_statement->value)));
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
    return Program(std::move(b));
}

CompactInterstageVariables::Config::Config() = default;

CompactInterstageVariables::Config::Config(const Config&) = default;

CompactInterstageVariables::Config::~Config() = default;

CompactInterstageVariables::Config& CompactInterstageVariables::Config::operator=(const Config&) =
    default;

}  // namespace tint::transform
