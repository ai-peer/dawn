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

#ifndef SRC_TINT_TRANSFORM_INTEGER_VALUE_RANGE_ANALYSIS_H_
#define SRC_TINT_TRANSFORM_INTEGER_VALUE_RANGE_ANALYSIS_H_

#include <limits>
#include <unordered_map>
#include <vector>

#include "src/tint/sem/variable.h"
#include "src/tint/transform/transform.h"

namespace tint::transform {

/// IntegerValueRangeAnalysis is an AST analysis that collects the integer variables which can be
/// proved to have finite constant value range according to the given AST, then when handling
/// robustness we can avoid adding index clamping on the indices that cannot be out of the array
/// bound.
class IntegerValueRangeAnalysis {
  public:
    /// Constructor
    IntegerValueRangeAnalysis();
    /// Destructor
    ~IntegerValueRangeAnalysis();

    /// Store the value range of an int32 or uint32 scalar. Currently we only focus on int32 or
    /// uint32 scalar and vectors, so using int64_t is enough to store these limitations.
    struct IntegerValueRange {
        int64_t maxValue = std::numeric_limits<int64_t>::max();
        int64_t minValue = std::numeric_limits<int64_t>::min();
    };

    /// Store all the integer variables that have finite range. Currently we only focus on int32 or
    /// uint32 scalar and vectors so the ranges of each component can be stored in a
    /// std::vector<IntegerValueRange>.
    using RangedIntegerVariablesMap =
        std::unordered_map<const tint::sem::Variable*, std::vector<IntegerValueRange>>;

    /// Run integer variable range analysis on `program` and return the result.
    /// @param program the input program
    /// @returns a RangedIntegerVariablesMap object that stores all the integer variables that have
    /// finite constant range according to the given AST.
    RangedIntegerVariablesMap Apply(const Program* program) const;

  private:
    struct State;
};

}  // namespace tint::transform

#endif  // SRC_TINT_TRANSFORM_INTEGER_VALUE_RANGE_ANALYSIS_H_
