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

#include "src/tint/lang/wgsl/ast/transform/offset_first_index.h"

#include <memory>
#include <unordered_map>
#include <utility>

#include "src/tint/lang/core/builtin_value.h"
#include "src/tint/lang/core/fluent_types.h"
#include "src/tint/lang/wgsl/program/clone_context.h"
#include "src/tint/lang/wgsl/program/program_builder.h"
#include "src/tint/lang/wgsl/resolver/resolve.h"
#include "src/tint/lang/wgsl/sem/function.h"
#include "src/tint/lang/wgsl/sem/member_accessor_expression.h"
#include "src/tint/lang/wgsl/sem/struct.h"
#include "src/tint/lang/wgsl/sem/variable.h"

using namespace tint::core::fluent_types;  // NOLINT

TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::OffsetFirstIndex);
TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::OffsetFirstIndex::Data);

namespace tint::ast::transform {
namespace {

// Push constant names
constexpr char kFirstVertexName[] = "tint_first_vertex";
constexpr char kFirstInstanceName[] = "tint_first_instance";

bool ShouldRun(const Program& program) {
    for (auto* fn : program.AST().Functions()) {
        if (fn->PipelineStage() == PipelineStage::kVertex) {
            return true;
        }
    }
    return false;
}

}  // namespace

OffsetFirstIndex::Data::Data(bool has_vtx_index, bool has_inst_index)
    : has_vertex_index(has_vtx_index), has_instance_index(has_inst_index) {}
OffsetFirstIndex::Data::Data(const Data&) = default;
OffsetFirstIndex::Data::~Data() = default;

OffsetFirstIndex::OffsetFirstIndex() = default;
OffsetFirstIndex::~OffsetFirstIndex() = default;

Transform::ApplyResult OffsetFirstIndex::Apply(const Program& src,
                                               const DataMap& inputs,
                                               DataMap& outputs) const {
    if (!ShouldRun(src)) {
        return SkipTransform;
    }

    const Config* cfg = inputs.Get<Config>();

    ProgramBuilder b;
    program::CloneContext ctx{&b, &src, /* auto_clone_symbols */ true};

    // Map of builtin usages
    std::unordered_map<const sem::Variable*, const char*> builtin_vars;
    std::unordered_map<const core::type::StructMember*, const char*> builtin_members;

    bool has_vertex_index = false;
    bool has_instance_index = false;

    // Traverse the AST scanning for builtin accesses via variables (includes
    // parameters) or structure member accesses.
    for (auto* node : ctx.src->ASTNodes().Objects()) {
        if (auto* var = node->As<Variable>()) {
            for (auto* attr : var->attributes) {
                if (auto* builtin_attr = attr->As<BuiltinAttribute>()) {
                    core::BuiltinValue builtin = src.Sem().Get(builtin_attr)->Value();
                    if (builtin == core::BuiltinValue::kVertexIndex) {
                        auto* sem_var = ctx.src->Sem().Get(var);
                        builtin_vars.emplace(sem_var, kFirstVertexName);
                        has_vertex_index = true;
                    }
                    if (builtin == core::BuiltinValue::kInstanceIndex) {
                        auto* sem_var = ctx.src->Sem().Get(var);
                        builtin_vars.emplace(sem_var, kFirstInstanceName);
                        has_instance_index = true;
                    }
                }
            }
        }
        if (auto* member = node->As<StructMember>()) {
            for (auto* attr : member->attributes) {
                if (auto* builtin_attr = attr->As<BuiltinAttribute>()) {
                    core::BuiltinValue builtin = src.Sem().Get(builtin_attr)->Value();
                    if (builtin == core::BuiltinValue::kVertexIndex) {
                        auto* sem_mem = ctx.src->Sem().Get(member);
                        builtin_members.emplace(sem_mem, kFirstVertexName);
                        has_vertex_index = true;
                    }
                    if (builtin == core::BuiltinValue::kInstanceIndex) {
                        auto* sem_mem = ctx.src->Sem().Get(member);
                        builtin_members.emplace(sem_mem, kFirstInstanceName);
                        has_instance_index = true;
                    }
                }
            }
        }
    }

    if (has_vertex_index || has_instance_index) {
        // Add push_constant variables
        if (has_vertex_index && cfg->first_vertex_location >= 0) {
            b.GlobalVar(kFirstVertexName, b.ty.u32(), core::AddressSpace::kPushConstant,
                        b.Location(u32(static_cast<uint32_t>(cfg->first_vertex_location))));
        }
        if (has_instance_index && cfg->first_instance_location >= 0) {
            b.GlobalVar(kFirstInstanceName, b.ty.u32(), core::AddressSpace::kPushConstant,
                        b.Location(u32(static_cast<uint32_t>(cfg->first_instance_location))));
        }

        // Fix up all references to the builtins with the offsets
        ctx.ReplaceAll([=, &ctx](const Expression* expr) -> const Expression* {
            if (auto* sem = ctx.src->Sem().GetVal(expr)) {
                if (auto* user = sem->UnwrapLoad()->As<sem::VariableUser>()) {
                    auto it = builtin_vars.find(user->Variable());
                    if (it != builtin_vars.end()) {
                        return ctx.dst->Add(ctx.CloneWithoutTransform(expr), it->second);
                    }
                }
                if (auto* access = sem->As<sem::StructMemberAccess>()) {
                    auto it = builtin_members.find(access->Member());
                    if (it != builtin_members.end()) {
                        return ctx.dst->Add(ctx.CloneWithoutTransform(expr), it->second);
                    }
                }
            }
            // Not interested in this expression. Just clone.
            return nullptr;
        });
    }

    outputs.Add<Data>(has_vertex_index, has_instance_index);

    ctx.Clone();
    return resolver::Resolve(b);
}

OffsetFirstIndex::Config::Config(int32_t first_vertex_loc, int32_t first_instance_loc)
    : first_vertex_location(first_vertex_loc), first_instance_location(first_instance_loc) {}

OffsetFirstIndex::Config::~Config() = default;

}  // namespace tint::ast::transform
