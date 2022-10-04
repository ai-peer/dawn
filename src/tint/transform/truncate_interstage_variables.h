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

#ifndef SRC_TINT_TRANSFORM_TRUNCATE_INTERSTAGE_VARIABLES_H_
#define SRC_TINT_TRANSFORM_TRUNCATE_INTERSTAGE_VARIABLES_H_

#include "src/tint/sem/binding_point.h"
#include "src/tint/transform/transform.h"
#include "src/tint/utils/bitset.h"

namespace tint::transform {

/// TruncateInterstageVariables is a Transform that truncate certain inter variables based on its
/// config.
class TruncateInterstageVariables final : public Castable<TruncateInterstageVariables, Transform> {
  public:
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

        // Indicate which interstage io locations are actually used by the later stage.
        utils::Bitset<16> interstage_locations;

        /// Reflect the fields of this class so that it can be used by tint::ForeachField()
        TINT_REFLECT(interstage_variables);
    };

    /// Constructor using a the configuration provided in the input Data
    TruncateInterstageVariables();

    /// Destructor
    ~TruncateInterstageVariables() override;

    /// @copydoc Transform::Apply
    ApplyResult Apply(const Program* program,
                      const DataMap& inputs,
                      DataMap& outputs) const override;
};

}  // namespace tint::transform

#endif  // SRC_TINT_TRANSFORM_TRUNCATE_INTERSTAGE_VARIABLES_H_
