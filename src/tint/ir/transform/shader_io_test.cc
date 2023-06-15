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

#include "src/tint/ir/transform/shader_io.h"

#include <utility>

#include "src/tint/ir/transform/test_helper.h"

namespace tint::ir::transform {
namespace {

using namespace tint::builtin::fluent_types;  // NOLINT
using namespace tint::number_suffixes;        // NOLINT

using IR_ShaderIOTest = TransformTest;

TEST_F(IR_ShaderIOTest, NoInputsOrOutputs) {
    auto* ep = b.Function("foo", ty.void_());
    mod.functions.Push(ep);

    auto sb = b.With(ep->StartTarget());
    sb.Return(ep);

    auto* src = R"(
%foo = func():void -> %b1 {
  %b1 = block {
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Transform::DataMap data;
    data.Add<ShaderIO::Config>(ShaderIO::Backend::kSpirv);
    Run<ShaderIO>(data);

    EXPECT_EQ(expect, str());
}

}  // namespace
}  // namespace tint::ir::transform
