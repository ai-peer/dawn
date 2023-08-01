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

#include "src/tint/lang/msl/writer/printer/helper_test.h"

namespace tint::msl::writer {
namespace {

using namespace tint::builtin::fluent_types;  // NOLINT
using namespace tint::number_suffixes;        // NOLINT

TEST_F(MslPrinterTest, Let_Constant) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Let("l", u32(42));
        b.Return(func);
    });

    ASSERT_TRUE(IRIsValid()) << Error();
    ASSERT_TRUE(generator_.Generate()) << generator_.Diagnostics().str();

    EXPECT_EQ(generator_.Result(), MetalHeader() + R"(
void foo() {
  uint l = 42u;
}
)");
}

TEST_F(MslPrinterTest, Let_SharedConstant) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Let("l1", u32(42));
        b.Let("l2", u32(42));
        b.Return(func);
    });

    ASSERT_TRUE(IRIsValid()) << Error();
    ASSERT_TRUE(generator_.Generate()) << generator_.Diagnostics().str();

    EXPECT_EQ(generator_.Result(), MetalHeader() + R"(
void foo() {
  uint l1 = 42u;
  uint l2 = 42u;
}
)");
}

}  // namespace
}  // namespace tint::msl::writer
