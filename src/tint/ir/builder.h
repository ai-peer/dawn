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

#ifndef SRC_TINT_IR_BUILDER_H_
#define SRC_TINT_IR_BUILDER_H_

#include <string>

#include "src/tint/ir/module.h"

// Forward Declarations
namespace tint {
class Program;
}  // namespace tint

namespace tint::ir {

/// The result produced when generating IR.
struct Result {
    /// Constructor
    Result();
    /// Copy constructor
    Result(const Result&);
    /// Destructor
    ~Result();

    /// True if generation was successful.
    bool success = false;

    /// The errors generated during generation, if any.
    std::string error;

    /// The generated ir.
    Module ir;
};

/// Builds an ir::Module from a given Program
class Builder {
 public:
  /// Builds an ir::Module from the given Program
  /// @param program the Program to use.
  /// @returns the |Result| of generating the IR.
  ///
  /// @note this assumes the program |IsValid|, and has had const-eval done so
  /// any abstract values have been calculated and converted into the relevant
  /// concrete types.
  static Result Build(const Program* program);

 private:
  /// Constructor
  Builder() = delete;
  /// Destructor
  ~Builder() = delete;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_BUILDER_H_
