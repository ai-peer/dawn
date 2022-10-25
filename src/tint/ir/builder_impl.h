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

#ifndef SRC_TINT_IR_BUILDER_IMPL_H_
#define SRC_TINT_IR_BUILDER_IMPL_H_

#include <string>

#include "src/tint/diagnostic/diagnostic.h"
#include "src/tint/ir/module.h"
#include "src/tint/utils/block_allocator.h"
#include "src/tint/ir/function.h"
#include "src/tint/ir/flow_node.h"

// Forward Declarations
namespace tint {
class Program;
}  // namespace tint

namespace tint::ir {

/// Builds an ir::Module from a given ast::Program
class BuilderImpl {
 public:
  /// Constructor
  BuilderImpl(const Program* program);
  /// Destructor
  ~BuilderImpl();

  /// Builds an ir::Module from the given Program
  /// @returns true on success, false otherwise
  bool Build();

  /// @returns the error
  std::string error() const { return diagnostics_.str(); }

  /// @returns the generated ir::Module
  Module ir() { return std::move(ir_); }

  /// @returns a function
  Function* Func(const ast::Function* f);

  /// Emits a function to the IR.
  /// @param func the function to emit
  /// @returns true if successful, false otherwise
  bool EmitFunction(const ast::Function* func);

 private:
  const Program* program_;

  Module ir_;
  diag::List diagnostics_;

  utils::Vector<size_t, 3> entry_points_;
  utils::BlockAllocator<Function> functions_;
  utils::BlockAllocator<FlowNode> flow_nodes_;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_BUILDER_H_
