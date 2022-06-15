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

#ifndef SRC_TINT_AST_VARIABLE_H_
#define SRC_TINT_AST_VARIABLE_H_

#include <utility>
#include <vector>

#include "src/tint/ast/access.h"
#include "src/tint/ast/attribute.h"
#include "src/tint/ast/expression.h"
#include "src/tint/ast/storage_class.h"

// Forward declarations
namespace tint::ast {
class BindingAttribute;
class GroupAttribute;
class LocationAttribute;
class Type;
}  // namespace tint::ast

namespace tint::ast {

/// VariableBindingPoint holds a group and binding attribute.
struct VariableBindingPoint {
    /// The `@group` part of the binding point
    const GroupAttribute* group = nullptr;
    /// The `@binding` part of the binding point
    const BindingAttribute* binding = nullptr;

    /// @returns true if the BindingPoint has a valid group and binding
    /// attribute.
    inline operator bool() const { return group && binding; }
};

/// Variable is the base class for Var, Let, Const, Override and Parameter.
///
/// An instance of this class represents one of five constructs in WGSL: "var"  declaration, "let"
/// declaration, "override" declaration, "const" declaration, or formal parameter to a function.
///
/// @see https://www.w3.org/TR/WGSL/#value-decls
class Variable : public Castable<Variable, Node> {
  public:
    /// Constructor
    /// @param program_id the identifier of the program that owns this node
    /// @param source the variable source
    /// @param sym the variable symbol
    /// @param type the declared variable type
    /// @param constructor the constructor expression
    /// @param attributes the variable attributes
    Variable(ProgramID program_id,
             const Source& source,
             const Symbol& sym,
             const ast::Type* type,
             const Expression* constructor,
             AttributeList attributes);

    /// Move constructor
    Variable(Variable&&);

    /// Destructor
    ~Variable() override;

    /// @returns the binding point information from the variable's attributes.
    /// @note binding points should only be applied to Var and Parameter types.
    VariableBindingPoint BindingPoint() const;

    /// The variable symbol
    const Symbol symbol;

    /// The declared variable type. This is null if the type is inferred, e.g.:
    ///   let f = 1.0;
    ///   var i = 1;
    const ast::Type* const type;

    /// The constructor expression or nullptr if none set
    const Expression* const constructor;

    /// The attributes attached to this variable
    const AttributeList attributes;
};

/// A "var" declaration is a name for typed storage.
///
/// Examples:
///
/// ```
///  // Declared outside a function, i.e. at module scope, requires
///  // a storage class.
///  var<workgroup> width : i32;     // no initializer
///  var<private> height : i32 = 3;  // with initializer
///
///  // A variable declared inside a function doesn't take a storage class,
///  // and maps to SPIR-V Function storage.
///  var computed_depth : i32;
///  var area : i32 = compute_area(width, height);
/// ```
///
/// @see https://www.w3.org/TR/WGSL/#var-decls
class Var final : public Castable<Var, Variable> {
  public:
    /// Create a 'var' variable
    /// @param program_id the identifier of the program that owns this node
    /// @param source the variable source
    /// @param sym the variable symbol
    /// @param type the declared variable type
    /// @param declared_storage_class the declared storage class
    /// @param declared_access the declared access control
    /// @param constructor the constructor expression
    /// @param attributes the variable attributes
    Var(ProgramID program_id,
        const Source& source,
        const Symbol& sym,
        const ast::Type* type,
        StorageClass declared_storage_class,
        Access declared_access,
        const Expression* constructor,
        AttributeList attributes);

    /// Move constructor
    Var(Var&&);

    /// Destructor
    ~Var() override;

    /// Clones this node and all transitive child nodes using the `CloneContext`
    /// `ctx`.
    /// @param ctx the clone context
    /// @return the newly cloned node
    const Var* Clone(CloneContext* ctx) const override;

    /// The declared storage class
    const StorageClass declared_storage_class;

    /// The declared access control
    const Access declared_access;
};

/// A "let" declaration is a name for a function-scoped runtime typed value.
///
/// Examples:
///
/// ```
///   let twice_depth : i32 = width + width;  // Must have initializer
/// ```
/// @see https://www.w3.org/TR/WGSL/#let-decls
class Let final : public Castable<Let, Variable> {
  public:
    /// Create a 'let' variable
    /// @param program_id the identifier of the program that owns this node
    /// @param source the variable source
    /// @param sym the variable symbol
    /// @param type the declared variable type
    /// @param constructor the constructor expression
    /// @param attributes the variable attributes
    Let(ProgramID program_id,
        const Source& source,
        const Symbol& sym,
        const ast::Type* type,
        const Expression* constructor,
        AttributeList attributes);

    /// Move constructor
    Let(Let&&);

    /// Destructor
    ~Let() override;

    /// Clones this node and all transitive child nodes using the `CloneContext`
    /// `ctx`.
    /// @param ctx the clone context
    /// @return the newly cloned node
    const Let* Clone(CloneContext* ctx) const override;
};

/// A "const" declaration is a name for a module-scoped or function-scoped creation-time value.
///
/// Examples:
///
/// ```
///   const n  = 123;                           // Abstract-integer typed constant
///   const pi = 3.14159265359;                 // Abstract-float typed constant
///   const max_f32 : f32 = 0x1.fffffep+127;    // f32 typed constant
/// ```
/// @see https://www.w3.org/TR/WGSL/#creation-time-consts
class Const final : public Castable<Const, Variable> {
  public:
    /// Create a 'const' creation-time value variable.
    /// @param program_id the identifier of the program that owns this node
    /// @param source the variable source
    /// @param sym the variable symbol
    /// @param type the declared variable type
    /// @param constructor the constructor expression
    /// @param attributes the variable attributes
    Const(ProgramID program_id,
          const Source& source,
          const Symbol& sym,
          const ast::Type* type,
          const Expression* constructor,
          AttributeList attributes);

    /// Move constructor
    Const(Const&&);

    /// Destructor
    ~Const() override;

    /// Clones this node and all transitive child nodes using the `CloneContext`
    /// `ctx`.
    /// @param ctx the clone context
    /// @return the newly cloned node
    const Const* Clone(CloneContext* ctx) const override;
};

/// An "override" declaration - a name for a pipeline-overridable constant.
/// Examples:
///
/// ```
///   override radius : i32 = 2;       // Can be overridden by name.
///   @id(5) override width : i32 = 2; // Can be overridden by ID.
///   override scale : f32;            // No default - must be overridden.
/// ```
/// @see https://www.w3.org/TR/WGSL/#override-decls
class Override final : public Castable<Override, Variable> {
  public:
    /// Create an 'override' pipeline-overridable constant.
    /// @param program_id the identifier of the program that owns this node
    /// @param source the variable source
    /// @param sym the variable symbol
    /// @param type the declared variable type
    /// @param constructor the constructor expression
    /// @param attributes the variable attributes
    Override(ProgramID program_id,
             const Source& source,
             const Symbol& sym,
             const ast::Type* type,
             const Expression* constructor,
             AttributeList attributes);

    /// Move constructor
    Override(Override&&);

    /// Destructor
    ~Override() override;

    /// Clones this node and all transitive child nodes using the `CloneContext`
    /// `ctx`.
    /// @param ctx the clone context
    /// @return the newly cloned node
    const Override* Clone(CloneContext* ctx) const override;
};

/// A formal parameter to a function - a name for a typed value to be passed into a function.
/// Example:
///
/// ```
///   fn twice(a: i32) -> i32 {  // "a:i32" is the formal parameter
///     return a + a;
///   }
/// ```
///
/// @see https://www.w3.org/TR/WGSL/#creation-time-consts
class Parameter final : public Castable<Parameter, Variable> {
  public:
    /// Create a 'const' creation-time value variable.
    /// @param program_id the identifier of the program that owns this node
    /// @param source the variable source
    /// @param sym the variable symbol
    /// @param type the declared variable type
    /// @param attributes the variable attributes
    Parameter(ProgramID program_id,
              const Source& source,
              const Symbol& sym,
              const ast::Type* type,
              AttributeList attributes);

    /// Move constructor
    Parameter(Parameter&&);

    /// Destructor
    ~Parameter() override;

    /// Clones this node and all transitive child nodes using the `CloneContext`
    /// `ctx`.
    /// @param ctx the clone context
    /// @return the newly cloned node
    const Parameter* Clone(CloneContext* ctx) const override;
};

/// A list of variables
using VariableList = std::vector<const Variable*>;

/// A list of `var` declarations
using VarList = std::vector<const Var*>;

/// A list of parameters
using ParameterList = std::vector<const Parameter*>;

}  // namespace tint::ast

#endif  // SRC_TINT_AST_VARIABLE_H_
