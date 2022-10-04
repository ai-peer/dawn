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

bool ShouldRun(const Program* program, const DataMap& data) {
    auto* cfg = data.Get<CompactInterstageVariables::Config>();
    if (!cfg) {
        // Skip if there is no data for what interstage variables need preserved.
        return false;
    }

    for (auto* func_ast : program->AST().Functions()) {
        if (!func_ast->IsEntryPoint()) {
            continue;
        }

        if (func_ast->PipelineStage() == ast::PipelineStage::kVertex) {
            // Run if function is vertex stage entry point, which is the only program stage with
            // interstage outputs now.
            return true;
        }
    }
    return false;
}

}  // anonymous namespace

CompactInterstageVariables::CompactInterstageVariables() = default;
CompactInterstageVariables::~CompactInterstageVariables() = default;

Transform::ApplyResult CompactInterstageVariables::Apply(const Program* src,
                                                         const DataMap& config,
                                                         DataMap& outputs) const {
    ProgramBuilder b;
    CloneContext ctx{&b, src, /* auto_clone_symbols */ true};

    if (!ShouldRun(src, config)) {
        return SkipTransform;
    }

    const auto* data = config.Get<Config>();

    auto& sem = ctx.src->Sem();
    auto& sym = ctx.src->Symbols();

    for (auto* func_ast : ctx.src->AST().Functions()) {
        if (!func_ast->IsEntryPoint()) {
            continue;
        }

        if (func_ast->PipelineStage() != ast::PipelineStage::kVertex) {
            // Currently only vertex stage could have interstage output variables that need
            // truncated.
            continue;
        }

        utils::Vector<const ast::StructMember*, 20> truncated_members;
        utils::Vector<const ast::StructMember*, 4> builtin_output_members;

        auto* str = sem.Get(func_ast)->ReturnType()->As<sem::Struct>();
        if (str) {
            // This transform is run after CanonicalizeEntryPointIO transform,
            // So it is guaranteed that entry point inputs are already grouped in a struct.
            const ast::Struct* struct_ty = str->Declaration();

            // Create a new struct for the interstage inputs & outputs.
            auto new_struct_sym = b.Symbols().New("TruncatedShaderIO");

            // Statements inside the mapping function from original shader io to trancated shader
            // io.
            utils::Vector<const ast::Statement*, 22> truncate_func_statements{
                b.Decl(b.Var("result", b.ty.type_name(new_struct_sym)))};

            for (auto* member : struct_ty->members) {
                if (auto* attr = ast::GetAttribute<ast::BuiltinAttribute>(member->attributes)) {
                    // For builtin interstage variables, copy as is.
                    std::string member_name = sym.NameFor(member->symbol);
                    builtin_output_members.Push(b.Member(member_name, ctx.Clone(member->type),
                                                         ctx.Clone(member->attributes)));
                    truncate_func_statements.Push(
                        b.Assign(b.MemberAccessor("result", ctx.Clone(member->symbol)),
                                 b.MemberAccessor("io", ctx.Clone(member->symbol))));

                    ctx.Remove(member->attributes, attr);
                } else if (auto* attr =
                               ast::GetAttribute<ast::LocationAttribute>(member->attributes)) {
                    // For user defined shader io, only preserve those with locations marked in the
                    // input.
                    auto* m = sem.Get(member);
                    uint32_t location = m->Location().value();
                    if (data->interstage_locations[location] == true) {
                        std::string member_name = sym.NameFor(member->symbol);
                        truncated_members.Push(b.Member(member_name, ctx.Clone(member->type),
                                                        ctx.Clone(member->attributes)));

                        truncate_func_statements.Push(
                            b.Assign(b.MemberAccessor("result", ctx.Clone(member->symbol)),
                                     b.MemberAccessor("io", ctx.Clone(member->symbol))));
                    }
                    ctx.Remove(member->attributes, attr);
                }
            }

            truncate_func_statements.Push(b.Return("result"));

            if (!builtin_output_members.IsEmpty()) {
                for (const ast::StructMember* builtin_member : builtin_output_members) {
                    truncated_members.Push(builtin_member);
                }
            }

            auto* new_structure = b.Structure(new_struct_sym, truncated_members);

            // Create the mapping function to truncate the shader io.
            auto mapping_fn_sym = b.Symbols().New("truncate_shader_output");
            auto* mapping_fn = b.Func(
                mapping_fn_sym, utils::Vector{b.Param("io", ctx.Clone(func_ast->return_type))},
                b.ty.type_name(new_struct_sym), truncate_func_statements);

            ctx.Replace(func_ast->return_type, b.ty.type_name(new_struct_sym));

            ctx.ReplaceAll([&](const ast::ReturnStatement* return_statement) {
                return b.Return(return_statement->source,
                                b.Call(mapping_fn_sym, ctx.Clone(return_statement->value)));
            });
        } else {
            TINT_UNREACHABLE(Transform, ctx.dst->Diagnostics());
        }
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
