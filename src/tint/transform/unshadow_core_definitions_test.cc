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

#include <string>
#include <vector>

#include "src/tint/sem/builtin_type.h"
#include "src/tint/transform/test_helper.h"
#include "src/tint/transform/unshadow_core_definitions.h"
#include "src/tint/type/short_name.h"

namespace tint::transform {
namespace {

std::vector<std::string> CoreDefinitionsList() {
    std::vector<std::string> out;
    for (auto name : type::kShortNameStrings) {
        out.push_back(name);
    }
    for (auto builtin : sem::kBuiltinTypes) {
        out.push_back(sem::str(builtin));
    }
    return out;
}

std::string ExpandTemplates(std::string_view name, std::string_view source) {
    auto out = utils::ReplaceAll(std::string(source), "$name", name);
    out =
        utils::ReplaceAll(out, "$unshadowed_type", std::string_view(name) == "i32" ? "u32" : "i32");
    return out;
}

using UnshadowCoreDefinitionsTest = TransformTest;

TEST_F(UnshadowCoreDefinitionsTest, EmptyModule) {
    auto* src = "";

    EXPECT_FALSE(ShouldRun<UnshadowCoreDefinitions>(src));
}

using UnshadowCoreDefinitionsParamTest = TransformTestWithParam<std::string>;

TEST_P(UnshadowCoreDefinitionsParamTest, ShadowCoreDefinitionWithAlias) {
    auto src = ExpandTemplates(GetParam(), R"(
type $name = $unshadowed_type;

var<private> v : $name;

fn f(p : $name) -> $name {
  return $name();
}
)");
    auto expect = ExpandTemplates(GetParam(), R"(
type $name_1 = $unshadowed_type;

var<private> v : $name_1;

fn f(p : $name_1) -> $name_1 {
  return $name_1();
}
)");

    auto got = Run<UnshadowCoreDefinitions>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_P(UnshadowCoreDefinitionsParamTest, ShadowCoreDefinitionWithStruct) {
    auto src = ExpandTemplates(GetParam(), R"(
struct $name {
  i : $unshadowed_type,
}

var<private> v : $name;

fn f(p : $name) -> $name {
  return $name();
}
)");
    auto expect = ExpandTemplates(GetParam(), R"(
struct $name_1 {
  i : $unshadowed_type,
}

var<private> v : $name_1;

fn f(p : $name_1) -> $name_1 {
  return $name_1();
}
)");

    auto got = Run<UnshadowCoreDefinitions>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_P(UnshadowCoreDefinitionsParamTest, ShadowCoreDefinitionWithVariable) {
    auto src = ExpandTemplates(GetParam(), R"(
const $name = 1;

const v = $name;

fn f() {
  const c = $name;
}
)");
    auto expect = ExpandTemplates(GetParam(), R"(
const $name_1 = 1;

const v = $name_1;

fn f() {
  const c = $name_1;
}
)");

    auto got = Run<UnshadowCoreDefinitions>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_P(UnshadowCoreDefinitionsParamTest, ShadowCoreDefinitionWithFunction) {
    auto src = ExpandTemplates(GetParam(), R"(
fn $name() {
}

fn f() {
  $name();
}
)");
    auto expect = ExpandTemplates(GetParam(), R"(
fn $name_1() {
}

fn f() {
  $name_1();
}
)");

    auto got = Run<UnshadowCoreDefinitions>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_P(UnshadowCoreDefinitionsParamTest, ShadowCoreDefinitionWithParameter) {
    auto src = ExpandTemplates(GetParam(), R"(
fn f($name : $unshadowed_type) {
  _ = $name;
}

fn g() {
  // Preserve type names / builtins if they're not shadowed
  var a : vec2i;
  var b : mat3x2f;
  var c : f32 = cos(1.0);
  var d : f32 = fract(1.0);
}
// Preserve type names if they're not shadowed
var<private> a : vec2i;

var<private> b : mat3x2f;
// Preserve builtins if they're not shadowed
const c = fract(1.0);

const d = cos(1.0);
)");
    auto expect = ExpandTemplates(GetParam(), R"(
fn f($name_1 : $unshadowed_type) {
  _ = $name_1;
}

fn g() {
  var a : vec2i;
  var b : mat3x2f;
  var c : f32 = cos(1.0);
  var d : f32 = fract(1.0);
}

var<private> a : vec2i;

var<private> b : mat3x2f;

const c = fract(1.0);

const d = cos(1.0);
)");

    auto got = Run<UnshadowCoreDefinitions>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_P(UnshadowCoreDefinitionsParamTest, ShadowCoreDefinitionWithLocalVar) {
    auto src = ExpandTemplates(GetParam(), R"(
fn f() {
  {
    var $name = 42;
    _ = $name;
  }
  // Preserve type names / builtins if they're not shadowed
  var a : vec2i;
  var b : mat3x2f;
  var c : f32 = cos(1.0);
  var d : f32 = fract(1.0);
}
// Preserve type names if they're not shadowed
var<private> a : vec2i;

var<private> b : mat3x2f;
// Preserve builtins if they're not shadowed
const c = fract(1.0);

const d = cos(1.0);
)");
    auto expect = ExpandTemplates(GetParam(), R"(
fn f() {
  {
    var $name_1 = 42;
    _ = $name_1;
  }
  var a : vec2i;
  var b : mat3x2f;
  var c : f32 = cos(1.0);
  var d : f32 = fract(1.0);
}

var<private> a : vec2i;

var<private> b : mat3x2f;

const c = fract(1.0);

const d = cos(1.0);
)");

    auto got = Run<UnshadowCoreDefinitions>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_P(UnshadowCoreDefinitionsParamTest, ShadowCoreDefinitionWithLocalConst) {
    auto src = ExpandTemplates(GetParam(), R"(
fn f() {
  {
    const $name = 42;
    _ = $name;
  }
  // Preserve type names / builtins if they're not shadowed
  var a : vec2i;
  var b : mat3x2f;
  var c : f32 = cos(1.0);
  var d : f32 = fract(1.0);
}
// Preserve type names if they're not shadowed
var<private> a : vec2i;

var<private> b : mat3x2f;
// Preserve builtins if they're not shadowed
const c = fract(1.0);

const d = cos(1.0);
)");
    auto expect = ExpandTemplates(GetParam(), R"(
fn f() {
  {
    const $name_1 = 42;
    _ = $name_1;
  }
  var a : vec2i;
  var b : mat3x2f;
  var c : f32 = cos(1.0);
  var d : f32 = fract(1.0);
}

var<private> a : vec2i;

var<private> b : mat3x2f;

const c = fract(1.0);

const d = cos(1.0);
)");

    auto got = Run<UnshadowCoreDefinitions>(src);

    EXPECT_EQ(expect, str(got));
}

INSTANTIATE_TEST_SUITE_P(UnshadowCoreDefinitionsParamTest,
                         UnshadowCoreDefinitionsParamTest,
                         testing::ValuesIn(CoreDefinitionsList()));

}  // namespace
}  // namespace tint::transform
