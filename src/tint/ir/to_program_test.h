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

#ifndef SRC_TINT_IR_TO_PROGRAM_TEST_H_
#define SRC_TINT_IR_TO_PROGRAM_TEST_H_

#include <string>

#include "src/tint/ir/ir_test_helper.h"

#if !TINT_BUILD_WGSL_WRITER
#error "to_program_test.h requires both the WGSL writer to be enabled"
#endif

namespace tint::ir::test {

/// Class used for IR to Program tests
class IRToProgramTest : public IRTestHelper {
  public:
    /// Checks that the IR module matches the expected WGSL after being converted using ToProgram()
    /// @param expected_wgsl the expected program's WGSL
    void Test(std::string_view expected_wgsl);
};

}  // namespace tint::ir::test

#endif  // SRC_TINT_IR_TO_PROGRAM_TEST_H_
