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

#include "src/tint/ast/extension.h"

#include "src/tint/ast/test_helper.h"

namespace tint {
namespace ast {
namespace {

using AstExtensionTest = TestHelper;

TEST_F(AstExtensionTest, Creation) {
  auto* ext = create<Extension>(
      Source{Source::Range{Source::Location{20, 2}, Source::Location{20, 5}}},
      "InternalExtensionForTesting");
  EXPECT_EQ(ext->source.range.begin.line, 20u);
  EXPECT_EQ(ext->source.range.begin.column, 2u);
  EXPECT_EQ(ext->source.range.end.line, 20u);
  EXPECT_EQ(ext->source.range.end.column, 5u);
  EXPECT_EQ(ext->kind, ast::Extension::Kind::kInternalExtensionForTesting);
}

TEST_F(AstExtensionTest, Creation_InvalidName) {
  auto* ext = create<Extension>(
      Source{Source::Range{Source::Location{20, 2}, Source::Location{20, 5}}},
      std::string());
  EXPECT_EQ(ext->source.range.begin.line, 20u);
  EXPECT_EQ(ext->source.range.begin.column, 2u);
  EXPECT_EQ(ext->source.range.end.line, 20u);
  EXPECT_EQ(ext->source.range.end.column, 5u);
  EXPECT_EQ(ext->kind, ast::Extension::Kind::kNotAnExtension);
}

TEST_F(AstExtensionTest, NameToKind_InvalidName) {
  EXPECT_EQ(ast::Extension::NameToKind(std::string()),
            ast::Extension::Kind::kNotAnExtension);
  EXPECT_EQ(ast::Extension::NameToKind("__ImpossibleExtensionName"),
            ast::Extension::Kind::kNotAnExtension);
  EXPECT_EQ(ast::Extension::NameToKind("123"),
            ast::Extension::Kind::kNotAnExtension);
}

TEST_F(AstExtensionTest, KindToName) {
  EXPECT_EQ(ast::Extension::KindToName(
                ast::Extension::Kind::kInternalExtensionForTesting),
            "InternalExtensionForTesting");
  EXPECT_EQ(ast::Extension::KindToName(ast::Extension::Kind::kNotAnExtension),
            std::string());
}

}  // namespace
}  // namespace ast
}  // namespace tint
