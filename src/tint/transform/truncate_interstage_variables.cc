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

#include "src/tint/transform/truncate_interstage_variables.h"

#include <memory>
#include <string>
#include <utility>

#include "src/tint/program_builder.h"
#include "src/tint/sem/call.h"
#include "src/tint/sem/function.h"
#include "src/tint/sem/member_accessor_expression.h"
#include "src/tint/sem/statement.h"
#include "src/tint/sem/variable.h"
#include "src/tint/text/unicode.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::TruncateInterstageVariables);
TINT_INSTANTIATE_TYPEINFO(tint::transform::TruncateInterstageVariables::Config);

namespace tint::transform {

TruncateInterstageVariables::TruncateInterstageVariables() = default;
TruncateInterstageVariables::~TruncateInterstageVariables() = default;

Transform::ApplyResult TruncateInterstageVariables::Apply(const Program* src,
                                                          const DataMap& config,
                                                          DataMap& outputs) const {
    ProgramBuilder b;
    CloneContext ctx{&b, src, /* auto_clone_symbols */ true};

    const auto* data = config.Get<Config>();
    if (data == nullptr) {
        b.Diagnostics().add_error(
            diag::System::Transform,
            "missing transform data for " +
                std::string(TypeInfo::Of<TruncateInterstageVariables>().name));
        return Program(std::move(b));
    }

    if (data->interstage_locations.IsEmpty()) {
        // Skip if interstage locations input is not marked.
        return SkipTransform;
    }

    auto& sem = ctx.src->Sem();
    auto& sym = ctx.src->Symbols();

    bool should_run = false;

    utils::Hashmap<const sem::Function*, Symbol, 4u> entry_point_functions_to_truncate_functions;
    utils::Hashmap<const sem::Struct*, utils::KeyValue<Symbol, Symbol>, 4u>
        old_shader_io_structs_to_new_struct_and_truncate_functions;

    for (auto* func_ast : ctx.src->AST().Functions()) {
        if (!func_ast->IsEntryPoint()) {
            continue;
        }

        if (func_ast->PipelineStage() != ast::PipelineStage::kVertex) {
            // Currently only vertex stage could have interstage output variables that need
            // truncated.
            continue;
        }

        auto* func_sem = sem.Get(func_ast);
        auto* str = func_sem->ReturnType()->As<sem::Struct>();

        if (!str) {
            TINT_ICE(Transform, ctx.dst->Diagnostics())
                << "Entrypoint function return type is non-struct.\n"
                << "TruncateInterstageVariables transform needs to run after Canonicalize "
                   "transform.";
            continue;
        }

        // This transform is run after CanonicalizeEntryPointIO transform,
        // So it is guaranteed that entry point inputs are already grouped in a struct.
        const ast::Struct* struct_ty = str->Declaration();

        // A prepass to check if any interstage variable locations in the entry point needs
        // truncating. If not we don't really need to handle this entry point.
        bool isAnyTruncating = false;
        for (auto* member : struct_ty->members) {
            if (ast::GetAttribute<ast::LocationAttribute>(member->attributes)) {
                auto* m = sem.Get(member);
                uint32_t location = m->Location().value();
                if (data->interstage_locations[location] == false) {
                    // Found location needs truncating.
                    isAnyTruncating = true;
                    break;
                }
            }
        }

        if (!isAnyTruncating) {
            continue;
        }

        // Now we are sure the transform needs to be run.
        should_run = true;

        utils::Vector<const ast::StructMember*, 20> truncated_members;
        utils::Vector<const ast::StructMember*, 4> builtin_output_members;

        // Get or create a new truncated struct/truncate function for the interstage inputs &
        // outputs.
        auto entry =
            old_shader_io_structs_to_new_struct_and_truncate_functions.GetOrCreate(str, [&] {
                auto new_struct_sym = b.Symbols().New();

                // Statements inside the mapping function from original shader io to trancated
                // shader io.
                utils::Vector<const ast::Statement*, 32> truncate_func_statements{
                    b.Decl(b.Var("result", b.ty.type_name(new_struct_sym)))};

                for (auto* member : struct_ty->members) {
                    if (ast::GetAttribute<ast::BuiltinAttribute>(member->attributes)) {
                        // For builtin interstage variables, copy as is.
                        std::string member_name = sym.NameFor(member->symbol);
                        builtin_output_members.Push(b.Member(member_name, ctx.Clone(member->type),
                                                             ctx.Clone(member->attributes)));
                        truncate_func_statements.Push(
                            b.Assign(b.MemberAccessor("result", ctx.Clone(member->symbol)),
                                     b.MemberAccessor("io", ctx.Clone(member->symbol))));
                    } else if (ast::GetAttribute<ast::LocationAttribute>(member->attributes)) {
                        // For user defined shader io, only preserve those with locations marked in
                        // the input.
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
                    }
                }

                truncate_func_statements.Push(b.Return("result"));

                if (!builtin_output_members.IsEmpty()) {
                    for (const ast::StructMember* builtin_member : builtin_output_members) {
                        truncated_members.Push(builtin_member);
                    }
                }

                // Create the new shader io struct.
                b.Structure(new_struct_sym, truncated_members);

                // Create the mapping function to truncate the shader io.
                auto mapping_fn_sym = b.Symbols().New("truncate_shader_output");
                b.Func(mapping_fn_sym,
                       utils::Vector{b.Param("io", ctx.Clone(func_ast->return_type))},
                       b.ty.type_name(new_struct_sym), truncate_func_statements);
                return utils::KeyValue<Symbol, Symbol>{new_struct_sym, mapping_fn_sym};
            });

        const auto& new_struct_sym = entry.key;
        const auto& mapping_fn_sym = entry.value;

        ctx.Replace(func_ast->return_type, b.ty.type_name(new_struct_sym));

        entry_point_functions_to_truncate_functions.Add(func_sem, mapping_fn_sym);
    }

    if (!should_run) {
        return SkipTransform;
    }

    // Replace return statements with new truncated shader IO struct
    ctx.ReplaceAll(
        [&](const ast::ReturnStatement* return_statement) -> const ast::ReturnStatement* {
            auto* return_sem = sem.Get(return_statement);
            if (auto* mapping_fn_sym =
                    entry_point_functions_to_truncate_functions.Find(return_sem->Function())) {
                return b.Return(return_statement->source,
                                b.Call(*mapping_fn_sym, ctx.Clone(return_statement->value)));
            }
            return nullptr;
        });

    // Remove IO attributes from old shader IO struct.
    for (auto it : old_shader_io_structs_to_new_struct_and_truncate_functions) {
        const ast::Struct* struct_ty = it.key->Declaration();
        for (auto* member : struct_ty->members) {
            for (auto* attr : member->attributes) {
                if (attr->IsAnyOf<ast::BuiltinAttribute, ast::LocationAttribute,
                                  ast::InterpolateAttribute, ast::InvariantAttribute>()) {
                    ctx.Remove(member->attributes, attr);
                }
            }
        }
    }

    ctx.Clone();
    return Program(std::move(b));
}

TruncateInterstageVariables::Config::Config() = default;

TruncateInterstageVariables::Config::Config(const Config&) = default;

TruncateInterstageVariables::Config::~Config() = default;

TruncateInterstageVariables::Config& TruncateInterstageVariables::Config::operator=(const Config&) =
    default;

}  // namespace tint::transform
