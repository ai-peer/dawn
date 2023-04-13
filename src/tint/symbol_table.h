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

#ifndef SRC_TINT_SYMBOL_TABLE_H_
#define SRC_TINT_SYMBOL_TABLE_H_

#include <string>
#include "utils/hashmap.h"

#include "src/tint/ast/module.h"
#include "src/tint/builtin/access.h"
#include "src/tint/builtin/builtin.h"
#include "src/tint/builtin/builtin_value.h"
#include "src/tint/builtin/function.h"
#include "src/tint/builtin/interpolation_sampling.h"
#include "src/tint/builtin/interpolation_type.h"
#include "src/tint/builtin/texel_format.h"
#include "src/tint/symbol.h"

namespace tint {

class SymbolTable;

/// UnresolvedIdentifier is the variant value used by ResolvedIdentifier
struct UnresolvedIdentifier {
    /// Name of the unresolved identifier
    std::string name;
};

/// ResolvedIdentifier holds the resolution of an ast::Identifier.
/// Can hold one of:
/// - UnresolvedIdentifier
/// - const ast::TypeDecl*  (as const ast::Node*)
/// - const ast::Variable*  (as const ast::Node*)
/// - const ast::Function*  (as const ast::Node*)
/// - builtin::Function
/// - builtin::Access
/// - builtin::AddressSpace
/// - builtin::Builtin
/// - builtin::BuiltinValue
/// - builtin::InterpolationSampling
/// - builtin::InterpolationType
/// - builtin::TexelFormat
class ResolvedIdentifier {
  public:
    /// Constructor
    /// @param value the resolved identifier value
    template <typename T>
    ResolvedIdentifier(T value) : value_(value) {}  // NOLINT(runtime/explicit)

    /// @return the UnresolvedIdentifier if the identifier was not resolved
    const UnresolvedIdentifier* Unresolved() const {
        if (auto n = std::get_if<UnresolvedIdentifier>(&value_)) {
            return n;
        }
        return nullptr;
    }

    /// @return the node pointer if the ResolvedIdentifier holds an AST node, otherwise nullptr
    const ast::Node* Node() const {
        if (auto n = std::get_if<const ast::Node*>(&value_)) {
            return *n;
        }
        return nullptr;
    }

    /// @return the builtin function if the ResolvedIdentifier holds builtin::Function, otherwise
    /// builtin::Function::kNone
    builtin::Function BuiltinFunction() const {
        if (auto n = std::get_if<builtin::Function>(&value_)) {
            return *n;
        }
        return builtin::Function::kNone;
    }

    /// @return the access if the ResolvedIdentifier holds builtin::Access, otherwise
    /// builtin::Access::kUndefined
    builtin::Access Access() const {
        if (auto n = std::get_if<builtin::Access>(&value_)) {
            return *n;
        }
        return builtin::Access::kUndefined;
    }

    /// @return the address space if the ResolvedIdentifier holds builtin::AddressSpace, otherwise
    /// builtin::AddressSpace::kUndefined
    builtin::AddressSpace AddressSpace() const {
        if (auto n = std::get_if<builtin::AddressSpace>(&value_)) {
            return *n;
        }
        return builtin::AddressSpace::kUndefined;
    }

    /// @return the builtin type if the ResolvedIdentifier holds builtin::Builtin, otherwise
    /// builtin::Builtin::kUndefined
    builtin::Builtin BuiltinType() const {
        if (auto n = std::get_if<builtin::Builtin>(&value_)) {
            return *n;
        }
        return builtin::Builtin::kUndefined;
    }

    /// @return the builtin value if the ResolvedIdentifier holds builtin::BuiltinValue, otherwise
    /// builtin::BuiltinValue::kUndefined
    builtin::BuiltinValue BuiltinValue() const {
        if (auto n = std::get_if<builtin::BuiltinValue>(&value_)) {
            return *n;
        }
        return builtin::BuiltinValue::kUndefined;
    }

    /// @return the texel format if the ResolvedIdentifier holds type::InterpolationSampling,
    /// otherwise type::InterpolationSampling::kUndefined
    builtin::InterpolationSampling InterpolationSampling() const {
        if (auto n = std::get_if<builtin::InterpolationSampling>(&value_)) {
            return *n;
        }
        return builtin::InterpolationSampling::kUndefined;
    }

    /// @return the texel format if the ResolvedIdentifier holds type::InterpolationType,
    /// otherwise type::InterpolationType::kUndefined
    builtin::InterpolationType InterpolationType() const {
        if (auto n = std::get_if<builtin::InterpolationType>(&value_)) {
            return *n;
        }
        return builtin::InterpolationType::kUndefined;
    }

    /// @return the texel format if the ResolvedIdentifier holds type::TexelFormat, otherwise
    /// type::TexelFormat::kUndefined
    builtin::TexelFormat TexelFormat() const {
        if (auto n = std::get_if<builtin::TexelFormat>(&value_)) {
            return *n;
        }
        return builtin::TexelFormat::kUndefined;
    }

    /// @param value the value to compare the ResolvedIdentifier to
    /// @return true if the ResolvedIdentifier is equal to @p value
    template <typename T>
    bool operator==(const T& value) const {
        if (auto n = std::get_if<T>(&value_)) {
            return *n == value;
        }
        return false;
    }

    /// @param other the other value to compare to this
    /// @return true if this ResolvedIdentifier and @p other are not equal
    template <typename T>
    bool operator!=(const T& other) const {
        return !(*this == other);
    }

    /// @param symbols the program's symbol table
    /// @param diagnostics diagnostics used to report ICEs
    /// @return a description of the resolved symbol
    std::string String(const SymbolTable& symbols, diag::List& diagnostics) const;

  private:
    std::variant<UnresolvedIdentifier,
                 const ast::Node*,
                 builtin::Function,
                 builtin::Access,
                 builtin::AddressSpace,
                 builtin::Builtin,
                 builtin::BuiltinValue,
                 builtin::InterpolationSampling,
                 builtin::InterpolationType,
                 builtin::TexelFormat>
        value_;
};

/// Holds mappings from symbols to their associated string names
class SymbolTable {
  public:
    /// Constructor
    /// @param program_id the identifier of the program that owns this symbol
    /// table
    explicit SymbolTable(tint::ProgramID program_id);
    /// Copy constructor
    SymbolTable(const SymbolTable&);
    /// Move Constructor
    SymbolTable(SymbolTable&&);
    /// Destructor
    ~SymbolTable();

    /// Copy assignment
    /// @param other the symbol table to copy
    /// @returns the new symbol table
    SymbolTable& operator=(const SymbolTable& other);
    /// Move assignment
    /// @param other the symbol table to move
    /// @returns the symbol table
    SymbolTable& operator=(SymbolTable&& other);

    /// Registers a name into the symbol table, returning the Symbol.
    /// @param name the name to register
    /// @returns the symbol representing the given name
    Symbol Register(const std::string& name);

    /// Returns the symbol for the given `name`
    /// @param name the name to lookup
    /// @returns the symbol for the name or Symbol() if not found.
    Symbol Get(const std::string& name) const;

    /// Returns the name for the given symbol
    /// @param symbol the symbol to retrieve the name for
    /// @returns the symbol name or "" if not found
    std::string NameFor(const Symbol symbol) const;

    /// Returns a new unique symbol with the given name, possibly suffixed with a
    /// unique number.
    /// @param name the symbol name
    /// @returns a new, unnamed symbol with the given name. If the name is already
    /// taken then this will be suffixed with an underscore and a unique numerical
    /// value
    Symbol New(std::string name = "");

    /// Foreach calls the callback function `F` for each symbol in the table.
    /// @param callback must be a function or function-like object with the
    /// signature: `void(Symbol, const std::string&)`
    template <typename F>
    void Foreach(F&& callback) const {
        for (auto it : symbol_to_name_) {
            callback(it.key, it.value);
        }
    }

    /// @returns the identifier of the Program that owns this symbol table.
    tint::ProgramID ProgramID() const { return program_id_; }

    /// Sets the symbol to the given resolved value
    void SetResolved(const Symbol& sym, ResolvedIdentifier ri) { symbol_to_resolved_.Add(sym, ri); }

    /// @returns an optional which contains the resolved identifier if found
    std::optional<ResolvedIdentifier> GetIfResolved(const Symbol& sym) const {
        return symbol_to_resolved_.Get(sym);
    }

  private:
    // The value to be associated to the next registered symbol table entry.
    uint32_t next_symbol_ = 1;

    utils::Hashmap<Symbol, std::string, 0> symbol_to_name_;
    utils::Hashmap<std::string, Symbol, 0> name_to_symbol_;
    utils::Hashmap<std::string, size_t, 0> last_prefix_to_index_;
    utils::Hashmap<Symbol, ResolvedIdentifier, 8> symbol_to_resolved_;

    tint::ProgramID program_id_;
};

/// @param symbol_table the SymbolTable
/// @returns the ProgramID that owns the given SymbolTable
inline ProgramID ProgramIDOf(const SymbolTable& symbol_table) {
    return symbol_table.ProgramID();
}

}  // namespace tint

#endif  // SRC_TINT_SYMBOL_TABLE_H_
