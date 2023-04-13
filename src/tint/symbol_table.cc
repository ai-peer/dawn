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

#include "src/tint/symbol_table.h"

#include "src/tint/ast/const.h"
#include "src/tint/ast/identifier.h"
#include "src/tint/ast/let.h"
#include "src/tint/ast/override.h"
#include "src/tint/ast/type_decl.h"
#include "src/tint/ast/var.h"
#include "src/tint/debug.h"
#include "src/tint/switch.h"

namespace tint {

SymbolTable::SymbolTable(tint::ProgramID program_id) : program_id_(program_id) {}

SymbolTable::SymbolTable(const SymbolTable&) = default;

SymbolTable::SymbolTable(SymbolTable&&) = default;

SymbolTable::~SymbolTable() = default;

SymbolTable& SymbolTable::operator=(const SymbolTable& other) = default;

SymbolTable& SymbolTable::operator=(SymbolTable&&) = default;

Symbol SymbolTable::Register(const std::string& name) {
    TINT_ASSERT(Symbol, !name.empty());

    auto it = name_to_symbol_.Find(name);
    if (it) {
        return *it;
    }

#if TINT_SYMBOL_STORE_DEBUG_NAME
    Symbol sym(next_symbol_, program_id_, name);
#else
    Symbol sym(next_symbol_, program_id_);
#endif
    ++next_symbol_;

    name_to_symbol_.Add(name, sym);
    symbol_to_name_.Add(sym, name);

    return sym;
}

Symbol SymbolTable::Get(const std::string& name) const {
    auto it = name_to_symbol_.Find(name);
    return it ? *it : Symbol();
}

std::string SymbolTable::NameFor(const Symbol symbol) const {
    TINT_ASSERT_PROGRAM_IDS_EQUAL(Symbol, program_id_, symbol);
    auto it = symbol_to_name_.Find(symbol);
    if (!it) {
        return symbol.to_str();
    }

    return *it;
}

Symbol SymbolTable::New(std::string prefix /* = "" */) {
    if (prefix.empty()) {
        prefix = "tint_symbol";
    }
    auto it = name_to_symbol_.Find(prefix);
    if (!it) {
        return Register(prefix);
    }

    size_t i = 0;
    auto last_prefix = last_prefix_to_index_.Find(prefix);
    if (last_prefix) {
        i = *last_prefix;
    }

    std::string name;
    do {
        ++i;
        name = prefix + "_" + std::to_string(i);
    } while (name_to_symbol_.Contains(name));

    if (last_prefix) {
        *last_prefix = i;
    } else {
        last_prefix_to_index_.Add(prefix, i);
    }

    return Register(name);
}

std::string ResolvedIdentifier::String(const SymbolTable& symbols, diag::List& diagnostics) const {
    if (auto* node = Node()) {
        return Switch(
            node,
            [&](const ast::TypeDecl* n) {  //
                return "type '" + symbols.NameFor(n->name->symbol) + "'";
            },
            [&](const ast::Var* n) {  //
                return "var '" + symbols.NameFor(n->name->symbol) + "'";
            },
            [&](const ast::Let* n) {  //
                return "let '" + symbols.NameFor(n->name->symbol) + "'";
            },
            [&](const ast::Const* n) {  //
                return "const '" + symbols.NameFor(n->name->symbol) + "'";
            },
            [&](const ast::Override* n) {  //
                return "override '" + symbols.NameFor(n->name->symbol) + "'";
            },
            [&](const ast::Function* n) {  //
                return "function '" + symbols.NameFor(n->name->symbol) + "'";
            },
            [&](const ast::Parameter* n) {  //
                return "parameter '" + symbols.NameFor(n->name->symbol) + "'";
            },
            [&](Default) {
                TINT_UNREACHABLE(Resolver, diagnostics)
                    << "unhandled ast::Node: " << node->TypeInfo().name;
                return "<unknown>";
            });
    }
    if (auto builtin_fn = BuiltinFunction(); builtin_fn != builtin::Function::kNone) {
        return "builtin function '" + utils::ToString(builtin_fn) + "'";
    }
    if (auto builtin_ty = BuiltinType(); builtin_ty != builtin::Builtin::kUndefined) {
        return "builtin type '" + utils::ToString(builtin_ty) + "'";
    }
    if (auto builtin_val = BuiltinValue(); builtin_val != builtin::BuiltinValue::kUndefined) {
        return "builtin value '" + utils::ToString(builtin_val) + "'";
    }
    if (auto access = Access(); access != builtin::Access::kUndefined) {
        return "access '" + utils::ToString(access) + "'";
    }
    if (auto addr = AddressSpace(); addr != builtin::AddressSpace::kUndefined) {
        return "address space '" + utils::ToString(addr) + "'";
    }
    if (auto type = InterpolationType(); type != builtin::InterpolationType::kUndefined) {
        return "interpolation type '" + utils::ToString(type) + "'";
    }
    if (auto smpl = InterpolationSampling(); smpl != builtin::InterpolationSampling::kUndefined) {
        return "interpolation sampling '" + utils::ToString(smpl) + "'";
    }
    if (auto fmt = TexelFormat(); fmt != builtin::TexelFormat::kUndefined) {
        return "texel format '" + utils::ToString(fmt) + "'";
    }
    if (auto* unresolved = Unresolved()) {
        return "unresolved identifier '" + unresolved->name + "'";
    }

    TINT_UNREACHABLE(Resolver, diagnostics) << "unhandled ResolvedIdentifier";
    return "<unknown>";
}

}  // namespace tint
