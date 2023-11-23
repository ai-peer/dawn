// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/tint/lang/spirv/writer/raise/negate_branch_conditions.h"

#include <utility>

#include "src/tint/lang/core/ir/transform/helper_test.h"

namespace tint::spirv::writer::raise {
namespace {

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

using SpirvWriter_NegateBranchConditionsTest = core::ir::transform::TransformTest;

TEST_F(SpirvWriter_NegateBranchConditionsTest, MatrixValuePassedToBuiltin) {
    auto* value = b.FunctionParam("value", ty.i32());
    auto* fn = b.Function("fn", ty.i32());
    b.Append(fn->Block(), [&] {
        auto* cond = b.LessThan(ty.bool_(), value, 4_i);
        auto* if_ = b.If(cond);
        b.Append(if_->True(), [&] {  //
            b.Return(fn, value);
        });
        b.Return(fn, 0_i);
    });

    auto* src = R"(
%fn = func():i32 -> %b1 {
  %b1 = block {
    %2:bool = lt %value, 4i
    if %2 [t: %b2] {  # if_1
      %b2 = block {  # true
        ret %value
      }
    }
    ret 0i
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%fn = func():i32 -> %b1 {
  %b1 = block {
    %2:bool = gte %value, 4i
    %4:bool = spirv.logical_not %2
    if %4 [t: %b2] {  # if_1
      %b2 = block {  # true
        ret %value
      }
    }
    ret 0i
  }
}
)";

    Run(NegateBranchConditions);

    EXPECT_EQ(expect, str());
}

// TODO: more tests

}  // namespace
}  // namespace tint::spirv::writer::raise
