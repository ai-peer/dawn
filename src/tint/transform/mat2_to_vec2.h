// Copyright 2021 The Tint Authors.
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

#ifndef SRC_TINT_TRANSFORM_MAT2_TO_VEC2_H_
#define SRC_TINT_TRANSFORM_MAT2_TO_VEC2_H_

#include <string>

#include "src/tint/ast/internal_attribute.h"
#include "src/tint/transform/transform.h"

namespace tint::transform {

/// Mat2ToVec2 is a transform that replaces mat2xY matrices in uniform structs
/// with multiple vec2 variables. Loads and stores to those struct members will
/// be automatically converted to/from variables of the matrix type.
class Mat2ToVec2 final : public Castable<Mat2ToVec2, Transform> {
  public:
    /// Constructor
    Mat2ToVec2();

    /// Destructor
    ~Mat2ToVec2() override;

  protected:
    /// Runs the transform using the CloneContext built for transforming a
    /// program. Run() is responsible for calling Clone() on the CloneContext.
    /// @param ctx the CloneContext primed with the input program and
    /// ProgramBuilder
    /// @param inputs optional extra transform-specific input data
    /// @param outputs optional extra transform-specific output data
    void Run(CloneContext& ctx, const DataMap& inputs, DataMap& outputs) const override;
};

}  // namespace tint::transform

#endif  // SRC_TINT_TRANSFORM_MAT2_TO_VEC2_H_
