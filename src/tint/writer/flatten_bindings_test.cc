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

#include "src/tint/writer/flatten_bindings.h"
#include "src/tint/program_builder.h"
#include "src/tint/resolver/resolver.h"
#include "src/tint/sem/variable.h"

#include "gtest/gtest.h"

namespace tint::writer {
namespace {

class FlattenBindingsTest : public ::testing::Test {};

TEST_F(FlattenBindingsTest, AlreadyFlat) {
  ProgramBuilder b;
  b.Global("a", b.ty.i32(), ast::StorageClass::kUniform,
           b.GroupAndBinding(0, 0));
  b.Global("b", b.ty.i32(), ast::StorageClass::kUniform,
           b.GroupAndBinding(0, 1));
  b.Global("c", b.ty.i32(), ast::StorageClass::kUniform,
           b.GroupAndBinding(0, 2));
  b.WrapInFunction();

  resolver::Resolver resolver(&b);

  Program program(std::move(b));
  ASSERT_TRUE(program.IsValid()) << program.Diagnostics().str();

  auto flattened = tint::writer::FlattenBindings(&program);
  EXPECT_FALSE(flattened);
}

TEST_F(FlattenBindingsTest, NotFlat) {
  ProgramBuilder b;
  b.Global("a", b.ty.i32(), ast::StorageClass::kUniform,
           b.GroupAndBinding(0, 0));
  b.Global("b", b.ty.i32(), ast::StorageClass::kUniform,
           b.GroupAndBinding(1, 1));
  b.Global("c", b.ty.i32(), ast::StorageClass::kUniform,
           b.GroupAndBinding(2, 2));
  b.WrapInFunction(b.Expr("a"), b.Expr("b"), b.Expr("c"));

  resolver::Resolver resolver(&b);

  Program program(std::move(b));
  ASSERT_TRUE(program.IsValid()) << program.Diagnostics().str();

  auto flattened = tint::writer::FlattenBindings(&program);
  EXPECT_TRUE(flattened);

  auto& vars = flattened->AST().GlobalVariables();
  EXPECT_EQ(vars[0]->BindingPoint().group->value, 0u);
  EXPECT_EQ(vars[0]->BindingPoint().binding->value, 0u);
  EXPECT_EQ(vars[1]->BindingPoint().group->value, 0u);
  EXPECT_EQ(vars[1]->BindingPoint().binding->value, 1u);
  EXPECT_EQ(vars[2]->BindingPoint().group->value, 0u);
  EXPECT_EQ(vars[2]->BindingPoint().binding->value, 2u);
}

}  // namespace
}  // namespace tint::writer
