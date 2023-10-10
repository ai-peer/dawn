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

#ifndef SRC_TINT_CMD_FUZZ_IR_IR_FUZZ_H_
#define SRC_TINT_CMD_FUZZ_IR_IR_FUZZ_H_

#include <string>

#include "src/tint/utils/containers/slice.h"
#include "src/tint/utils/macros/static_init.h"

// Forward declarations
namespace tint::core::ir {
class Module;
}

namespace tint::fuzz::ir {

/// IRFuzzer describes a fuzzer function that takes a WGSL program as input
struct IRFuzzer {
    /// The function signature
    using Fn = void(const ::tint::core::ir::Module&);

    /// Name of the fuzzer function
    std::string_view name;
    /// The fuzzer function pointer
    Fn* fn = nullptr;
};

/// Runs all the registered IR fuzzers with the supplied module
/// @param ir the IR module
void Run(::tint::core::ir::Module& ir);

/// Registers the fuzzer function with the IR fuzzer executable.
/// @param fuzzer the fuzzer
void Register(const IRFuzzer& fuzzer);

/// TINT_IR_FUZZER registers the fuzzer function to run as part of `tint_ir_fuzzer`
#define TINT_IR_FUZZER(FUNCTION) TINT_STATIC_INIT(::tint::fuzz::ir::Register({#FUNCTION, FUNCTION}))

}  // namespace tint::fuzz::ir

#endif  // SRC_TINT_CMD_FUZZ_IR_IR_FUZZ_H_
