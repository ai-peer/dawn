// Copyright 2023 The Tint Authors.
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

#ifndef SRC_TINT_TRANSFORM_UNSHADOW_CORE_DEFINITIONS_H_
#define SRC_TINT_TRANSFORM_UNSHADOW_CORE_DEFINITIONS_H_

#include "src/tint/transform/transform.h"

namespace tint::transform {

/// Transform that renames declarations that shadow core language types and intrinsics.
/// This acts as a sanitizer transform, ensuring that all downstream logic does not have to deal
/// with the possibility of core language types and intrinsics being shadowed.
class UnshadowCoreDefinitions final : public Castable<UnshadowCoreDefinitions, Transform> {
  public:
    /// Constructor
    UnshadowCoreDefinitions();

    /// Destructor
    ~UnshadowCoreDefinitions() override;

    /// @copydoc Transform::Apply
    ApplyResult Apply(const Program* program,
                      const DataMap& inputs,
                      DataMap& outputs) const override;

  private:
    struct State;
};

}  // namespace tint::transform

#endif  // SRC_TINT_TRANSFORM_UNSHADOW_CORE_DEFINITIONS_H_
