// Copyright 2020 The Tint Authors.
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

#include "src/tint/ast/variable.h"

#include "src/tint/program_builder.h"
#include "src/tint/sem/variable.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::Variable);
TINT_INSTANTIATE_TYPEINFO(tint::ast::Var);
TINT_INSTANTIATE_TYPEINFO(tint::ast::Let);
TINT_INSTANTIATE_TYPEINFO(tint::ast::Override);
TINT_INSTANTIATE_TYPEINFO(tint::ast::Parameter);

namespace tint::ast {

////////////////////////////////////////////////////////////////////////////////
// Variable
////////////////////////////////////////////////////////////////////////////////
Variable::Variable(ProgramID pid,
                   const Source& src,
                   const Symbol& sym,
                   const ast::Type* ty,
                   const Expression* ctor,
                   AttributeList attrs)
    : Base(pid, src), symbol(sym), type(ty), constructor(ctor), attributes(std::move(attrs)) {
    TINT_ASSERT(AST, symbol.IsValid());
    TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, symbol, program_id);
    TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, constructor, program_id);
}

Variable::Variable(Variable&&) = default;

Variable::~Variable() = default;

VariableBindingPoint Variable::BindingPoint() const {
    const GroupAttribute* group = nullptr;
    const BindingAttribute* binding = nullptr;
    for (auto* attr : attributes) {
        if (auto* g = attr->As<GroupAttribute>()) {
            group = g;
        } else if (auto* b = attr->As<BindingAttribute>()) {
            binding = b;
        }
    }
    return VariableBindingPoint{group, binding};
}

////////////////////////////////////////////////////////////////////////////////
// Var
////////////////////////////////////////////////////////////////////////////////
Var::Var(ProgramID pid,
         const Source& src,
         const Symbol& sym,
         const ast::Type* ty,
         StorageClass storage_class,
         Access access,
         const Expression* ctor,
         AttributeList attrs)
    : Base(pid, src, sym, ty, ctor, attrs),
      declared_storage_class(storage_class),
      declared_access(access) {}

Var::Var(Var&&) = default;

Var::~Var() = default;

const Var* Var::Clone(CloneContext* ctx) const {
    auto src = ctx->Clone(source);
    auto sym = ctx->Clone(symbol);
    auto* ty = ctx->Clone(type);
    auto* ctor = ctx->Clone(constructor);
    auto attrs = ctx->Clone(attributes);
    return ctx->dst->create<Var>(src, sym, ty, declared_storage_class, declared_access, ctor,
                                 attrs);
}

////////////////////////////////////////////////////////////////////////////////
// Let
////////////////////////////////////////////////////////////////////////////////
Let::Let(ProgramID pid,
         const Source& src,
         const Symbol& sym,
         const ast::Type* ty,
         const Expression* ctor,
         AttributeList attrs)
    : Base(pid, src, sym, ty, ctor, attrs) {
    TINT_ASSERT(AST, ctor != nullptr);
}

Let::Let(Let&&) = default;

Let::~Let() = default;

const Let* Let::Clone(CloneContext* ctx) const {
    auto src = ctx->Clone(source);
    auto sym = ctx->Clone(symbol);
    auto* ty = ctx->Clone(type);
    auto* ctor = ctx->Clone(constructor);
    auto attrs = ctx->Clone(attributes);
    return ctx->dst->create<Let>(src, sym, ty, ctor, attrs);
}

////////////////////////////////////////////////////////////////////////////////
// Override
////////////////////////////////////////////////////////////////////////////////
Override::Override(ProgramID pid,
                   const Source& src,
                   const Symbol& sym,
                   const ast::Type* ty,
                   const Expression* ctor,
                   AttributeList attrs)
    : Base(pid, src, sym, ty, ctor, attrs) {}

Override::Override(Override&&) = default;

Override::~Override() = default;

const Override* Override::Clone(CloneContext* ctx) const {
    auto src = ctx->Clone(source);
    auto sym = ctx->Clone(symbol);
    auto* ty = ctx->Clone(type);
    auto* ctor = ctx->Clone(constructor);
    auto attrs = ctx->Clone(attributes);
    return ctx->dst->create<Override>(src, sym, ty, ctor, attrs);
}

////////////////////////////////////////////////////////////////////////////////
// Parameter
////////////////////////////////////////////////////////////////////////////////
Parameter::Parameter(ProgramID pid,
                     const Source& src,
                     const Symbol& sym,
                     const ast::Type* ty,
                     AttributeList attrs)
    : Base(pid, src, sym, ty, nullptr, attrs) {}

Parameter::Parameter(Parameter&&) = default;

Parameter::~Parameter() = default;

const Parameter* Parameter::Clone(CloneContext* ctx) const {
    auto src = ctx->Clone(source);
    auto sym = ctx->Clone(symbol);
    auto* ty = ctx->Clone(type);
    auto attrs = ctx->Clone(attributes);
    return ctx->dst->create<Parameter>(src, sym, ty, attrs);
}

}  // namespace tint::ast
