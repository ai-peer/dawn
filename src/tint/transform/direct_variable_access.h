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

#ifndef SRC_TINT_TRANSFORM_DIRECT_VARIABLE_ACCESS_H_
#define SRC_TINT_TRANSFORM_DIRECT_VARIABLE_ACCESS_H_

#include "src/tint/transform/transform.h"

namespace tint::transform {

/// DirectVariableAccess is a transform that...
class DirectVariableAccess final : public Castable<DirectVariableAccess, Transform> {
  public:
    /// Constructor
    DirectVariableAccess();
    /// Destructor
    ~DirectVariableAccess() override;

    /// Options adjusts the behaviour of the transform.
    struct Options {
        /// If true, then 'private' sub-object pointer arguments will be transformed.
        bool transform_private = false;
        /// If true, then 'function' sub-object pointer arguments will be transformed.
        bool transform_function = false;
    };

    /// Config is consumed by the DirectVariableAccess transform.
    /// Config specifies the behavior of the transform.
    struct Config final : public Castable<Data, transform::Data> {
        /// Constructor
        /// @param options behavior of the transform
        explicit Config(const Options& options);
        /// Destructor
        ~Config() override;

        /// The transform behavior options
        const Options options;
    };

    /// @copydoc Transform::Apply
    ApplyResult Apply(const Program* program,
                      const DataMap& inputs,
                      DataMap& outputs) const override;

  private:
    struct State;
};

}  // namespace tint::transform

#endif  // SRC_TINT_TRANSFORM_DIRECT_VARIABLE_ACCESS_H_
