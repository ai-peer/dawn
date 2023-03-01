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

#ifndef SRC_TINT_TRANSFORM_ROBUSTNESS_H_
#define SRC_TINT_TRANSFORM_ROBUSTNESS_H_

#include <unordered_map>

#include "src/tint/builtin/address_space.h"
#include "src/tint/transform/transform.h"

// Forward declarations
namespace tint::ast {
class IndexAccessorExpression;
class CallExpression;
}  // namespace tint::ast

namespace tint::transform {

/// This transform is responsible for clamping all array accesses to be within
/// the bounds of the array. Any access before the start of the array will clamp
/// to zero and any access past the end of the array will clamp to
/// (array length - 1).
/// @note Robustness must come after:
///       * PromoteSideEffectsToDecl as Robustness requires side-effecting expressions to be hoisted
///         to their own statements.
///       Robustness must come before:
///       * BuiltinPolyfill as 'clamp' and binary operators may need to be polyfilled.
///       * CanonicalizeEntryPointIO as the transform does not support the 'in' and 'out' address
///         spaces.
class Robustness final : public Castable<Robustness, Transform> {
  public:
    /// Robustness action for out-of-bounds indexing.
    enum class Action {
        /// Do nothing to prevent the out-of-bounds action.
        kIgnore,
        /// Clamp the index to be within bounds.
        kClamp,
        /// Do not execute the read or write if the index is out-of-bounds.
        kPredicate,

        /// The default action
        kDefault = kPredicate,
    };

    /// Configuration options for the transform
    struct Config final : public Castable<Config, Data> {
        /// Constructor
        Config();

        /// Copy constructor
        Config(const Config&);

        /// Destructor
        ~Config() override;

        /// Assignment operator
        /// @returns this Config
        Config& operator=(const Config&);

        /// Robustness action for values
        Action value_action = Action::kDefault;
        /// Robustness action for variables in the 'function' address space
        Action function_action = Action::kDefault;
        /// Robustness action for variables in the 'handle' address space (e.g. textures)
        Action handle_action = Action::kDefault;
        /// Robustness action for variables in the 'private' address space
        Action private_action = Action::kDefault;
        /// Robustness action for variables in the 'push_constant' address space
        Action push_constant_action = Action::kDefault;
        /// Robustness action for variables in the 'storage' address space
        Action storage_action = Action::kDefault;
        /// Robustness action for variables in the 'uniform' address space
        Action uniform_action = Action::kDefault;
        /// Robustness action for variables in the 'workgroup' address space
        Action workgroup_action = Action::kDefault;
    };

    /// Constructor
    Robustness();
    /// Destructor
    ~Robustness() override;

    /// @copydoc Transform::Apply
    ApplyResult Apply(const Program* program,
                      const DataMap& inputs,
                      DataMap& outputs) const override;

  private:
    struct State;
};

}  // namespace tint::transform

#endif  // SRC_TINT_TRANSFORM_ROBUSTNESS_H_
