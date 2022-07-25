
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

#include "src/tint/ast/extension.h"

#include "gtest/gtest.h"

#include "src/tint/utils/string.h"

namespace tint::ast {
namespace {

TEST(ExtensionTest, NameToKind_InvalidName) {
    EXPECT_EQ(ParseExtension("f16"), Extension::kF16);
    EXPECT_EQ(ParseExtension(""), Extension::kInvalid);
    EXPECT_EQ(ParseExtension("__ImpossibleExtensionName"), Extension::kInvalid);
    EXPECT_EQ(ParseExtension("123"), Extension::kInvalid);
}

TEST(ExtensionTest, KindToName) {
    EXPECT_EQ(utils::ToString(Extension::kF16), "f16");
    EXPECT_EQ(utils::ToString(Extension::kInvalid), "invalid");
}

}  // namespace
}  // namespace tint::ast
