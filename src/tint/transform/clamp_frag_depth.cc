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

#include "src/tint/transform/clamp_frag_depth.h"

#include "src/tint/ast/attribute.h"
#include "src/tint/ast/builtin_attribute.h"
#include "src/tint/ast/builtin_value.h"
#include "src/tint/ast/function.h"
#include "src/tint/ast/module.h"
#include "src/tint/ast/struct.h"
#include "src/tint/ast/type.h"
#include "src/tint/program_builder.h"
#include "src/tint/sem/function.h"
#include "src/tint/sem/statement.h"
#include "src/tint/sem/struct.h"
#include "src/tint/utils/vector.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::ClampFragDepth);

namespace tint::transform {

namespace {

bool IsFragDepth(const ast::Attribute* attribute) {
    if (auto* builtin_attribute = attribute->As<ast::BuiltinAttribute>()) {
        return builtin_attribute->builtin == ast::BuiltinValue::kFragDepth;
    }
    return false;
}

bool ReturnsFragDepthAsValue(const ast::Function* fn) {
    for (auto* attribute : fn->return_type_attributes) {
        if (IsFragDepth(attribute)) {
            return true;
        }
    }

    return false;
}

bool ReturnsFragDepthInStruct(const sem::Info& sem, const ast::Function* fn) {
    if (auto* struct_ty = sem.Get(fn)->ReturnType()->As<sem::Struct>()) {
        for (auto* member : struct_ty->Members()) {
            for (auto* attribute : member->Declaration()->attributes) {
                if (IsFragDepth(attribute)) {
                    return true;
                }
            }
        }
    }

    return false;
}

}  // anonymous namespace

ClampFragDepth::ClampFragDepth() = default;
ClampFragDepth::~ClampFragDepth() = default;

bool ClampFragDepth::ShouldRun(const Program* program, const DataMap&) const {
    auto& sem = program->Sem();

    for (auto* fn : program->AST().Functions()) {
        if (fn->PipelineStage() == ast::PipelineStage::kFragment &&
            (ReturnsFragDepthAsValue(fn) || ReturnsFragDepthInStruct(sem, fn))) {
            return true;
        }
    }

    return false;
}

void ClampFragDepth::Run(CloneContext& ctx, const DataMap&, DataMap&) const {
    // Abort on any use of push constants in the module.
    for (auto* global : ctx.src->AST().GlobalVariables()) {
        if (auto* var = global->As<ast::Var>()) {
            if (var->declared_address_space == ast::AddressSpace::kPushConstant) {
                TINT_ICE(Transform, ctx.dst->Diagnostics())
                    << "ClampFragDepth doesn't know how to handle module that already use push "
                       "constants.";
            }
        }
    }

    auto& b = *ctx.dst;
    auto& sem = ctx.src->Sem();
    auto& sym = ctx.src->Symbols();

    // At least one entry-point needs clamping. Add the following to the module:
    //
    //   enable chromium_experimental_push_constant;
    //
    //   struct FragDepthClampArgs {
    //       min : f32,
    //       max : f32,
    //   }
    //   var<push_constant> frag_depth_clamp_args : FragDepthClampArgs;
    //
    //   fn clamp_frag_depth(v : f32) -> f32 {
    //       return clamp(v, frag_depth_clamp_args.min, frag_depth_clamp_args.max);
    //   }
    b.Enable(ast::Extension::kChromiumExperimentalPushConstant);

    b.Structure(b.Symbols().New("FragDepthClampArgs"),
                utils::Vector{b.Member("min", b.ty.f32()), b.Member("max", b.ty.f32())});

    auto args_sym = b.Symbols().New("frag_depth_clamp_args");
    b.GlobalVar(args_sym, b.ty.type_name("FragDepthClampArgs"), ast::AddressSpace::kPushConstant);

    auto base_fn_sym = b.Symbols().New("clamp_frag_depth");
    b.Func(base_fn_sym, utils::Vector{b.Param("v", b.ty.f32())}, b.ty.f32(),
           utils::Vector{b.Return(b.Call("clamp", "v", b.MemberAccessor(args_sym, "min"),
                                         b.MemberAccessor(args_sym, "max")))});

    // Precompute the set of functions that need special handling so we don't have to walk their
    // outputs for every return statement.
    utils::Hashset<const ast::Function*, 4u> returns_frag_depth_as_value;
    utils::Hashset<const ast::Function*, 4u> returns_frag_depth_in_struct;
    utils::Hashmap<const ast::Struct*, Symbol, 4u> io_structs_to_clamp_frag_depth_sym;

    for (auto* fn : ctx.src->AST().Functions()) {
        if (fn->PipelineStage() != ast::PipelineStage::kFragment) {
            continue;
        }

        if (ReturnsFragDepthAsValue(fn)) {
            returns_frag_depth_as_value.Add(fn);
        } else if (ReturnsFragDepthInStruct(sem, fn)) {
            returns_frag_depth_in_struct.Add(fn);

            // At most once per I/O struct, add the conversion function:
            //
            //   fn clamp_frag_depth_S(s : S) -> S {
            //       return S(s.first, s.second, clamp_frag_depth(s.frag_depth), s.last);
            //   }
            auto* return_ty = fn->return_type;
            auto* struct_ty = sem.Get(fn)->ReturnType()->As<sem::Struct>()->Declaration();
            if (io_structs_to_clamp_frag_depth_sym.Contains(struct_ty)) {
                continue;
            }

            auto fn_sym = b.Symbols().New("clamp_frag_depth_" +
                                          sym.NameFor(return_ty->As<ast::TypeName>()->name));

            utils::Vector<const ast::Expression*, 8u> constructor_args;
            for (auto* member : struct_ty->members) {
                constructor_args.Push(b.MemberAccessor("s", ctx.Clone(member->symbol)));
            }
            b.Func(fn_sym, utils::Vector{b.Param("s", ctx.Clone(return_ty))}, ctx.Clone(return_ty),
                   utils::Vector{b.Return(b.Construct(ctx.Clone(return_ty), constructor_args))});

            io_structs_to_clamp_frag_depth_sym.Add(struct_ty, fn_sym);
        }
    }

    // Replace the return statements `return expr` with `return clamp_frag_depth(expr)`.
    ctx.ReplaceAll([&](const ast::ReturnStatement* stmt) -> const ast::ReturnStatement* {
        auto* fn = sem.Get(stmt)->Function()->Declaration();

        if (returns_frag_depth_as_value.Contains(fn)) {
            return b.Return(stmt->source, b.Call(base_fn_sym, ctx.Clone(stmt->value)));
        }

        if (returns_frag_depth_in_struct.Contains(fn)) {
            auto* struct_ty = sem.Get(fn)->ReturnType()->As<sem::Struct>()->Declaration();
            auto fn_sym = *io_structs_to_clamp_frag_depth_sym.Find(struct_ty);
            return b.Return(stmt->source, b.Call(fn_sym, ctx.Clone(stmt->value)));
        }

        return nullptr;
    });

    ctx.Clone();
}

}  // namespace tint::transform
