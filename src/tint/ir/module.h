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

#ifndef SRC_TINT_IR_MODULE_H_
#define SRC_TINT_IR_MODULE_H_

#include "src/tint/utils/vector.h"
#include "src/tint/ir/function.h"

// Forward Declarations
namespace tint {
class Program;
}  // namespace tint

namespace tint::ir {

/// Main module class for the IR.
class Module {
 public:
  /// Constructor
  /// @param program the program this module was constructed from
  Module(const Program* program);
  /// Move constructor
  Module(Module&& o);
  /// Destructor
  ~Module();

  /// Move assign
  Module& operator=(Module&&);

  /// List of functions in the program
  utils::Vector<Function*, 8> functions;
  /// List of indexes into the functions list for the entry points
  utils::Vector<size_t, 8> entry_points;

 private:
  const Program* program_;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_MODULE_H_
