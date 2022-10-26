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

#include "src/tint/transform/packed_vec3.h"

#include <string>
#include <utility>
#include <vector>

#include "src/tint/transform/test_helper.h"
#include "src/tint/utils/string.h"

namespace tint::transform {
namespace {

using PackedVec3Test = TransformTest;

TEST_F(PackedVec3Test, ShouldRun_EmptyModule) {
    auto* src = R"()";

    EXPECT_FALSE(ShouldRun<PackedVec3>(src));
}

TEST_F(PackedVec3Test, ShouldRun_NonHostSharableStruct) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

fn f() {
  var v : S; // function address-space - not host sharable
}
)";

    EXPECT_FALSE(ShouldRun<PackedVec3>(src));
}

TEST_F(PackedVec3Test, ShouldRun_HostSharableStruct) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<uniform> P : S; // Host sharable
)";

    EXPECT_TRUE(ShouldRun<PackedVec3>(src));
}

TEST_F(PackedVec3Test, UniformAddressSpace) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<uniform> P : S;

fn f() {
  let x = P.v;
}
)";

    auto* expect = R"(
struct S {
  @internal(packed_vector)
  v : vec3<f32>,
}

@group(0) @binding(0) var<uniform> P : S;

fn f() {
  let x = vec3<f32>(P.v);
}
)";

    DataMap data;
    auto got = Run<PackedVec3>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(PackedVec3Test, StorageAddressSpace) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = P.v;
}
)";

    auto* expect = R"(
struct S {
  @internal(packed_vector)
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = vec3<f32>(P.v);
}
)";

    DataMap data;
    auto got = Run<PackedVec3>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(PackedVec3Test, MixedAddressSpace) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  var f : S;
  let x = f.v;
}
)";

    auto* expect = R"(
struct S {
  @internal(packed_vector)
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  var f : S;
  let x = vec3<f32>(f.v);
}
)";

    DataMap data;
    auto got = Run<PackedVec3>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(PackedVec3Test, ReadMemberAccessChain) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = P.v.yz.x;
}
)";

    auto* expect = R"(
struct S {
  @internal(packed_vector)
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = P.v.yz.x;
}
)";

    DataMap data;
    auto got = Run<PackedVec3>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(PackedVec3Test, ReadVector) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = P.v;
}
)";

    auto* expect = R"(
struct S {
  @internal(packed_vector)
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = vec3<f32>(P.v);
}
)";

    DataMap data;
    auto got = Run<PackedVec3>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(PackedVec3Test, ReadIndexAccessor) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = P.v[1];
}
)";

    auto* expect = R"(
struct S {
  @internal(packed_vector)
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = P.v[1];
}
)";

    DataMap data;
    auto got = Run<PackedVec3>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(PackedVec3Test, ReadViaStructPtrDirect) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = (*(&(*(&P)))).v;
}
)";

    auto* expect = R"(
struct S {
  @internal(packed_vector)
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = vec3<f32>((*(&(*(&(P))))).v);
}
)";

    DataMap data;
    auto got = Run<PackedVec3>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(PackedVec3Test, ReadViaVectorPtrDirect) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = *(&(*(&(P.v))));
}
)";

    auto* expect = R"(
struct S {
  @internal(packed_vector)
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = vec3<f32>(*(&(*(&(P.v)))));
}
)";

    DataMap data;
    auto got = Run<PackedVec3>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(PackedVec3Test, ReadViaStructPtrViaLet) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let p0 = &P;
  let p1 = &(*(p0));
  let x = (*p1).v;
}
)";

    auto* expect = R"(
struct S {
  @internal(packed_vector)
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let p0 = &(P);
  let p1 = &(*(p0));
  let x = vec3<f32>((*(p1)).v);
}
)";

    DataMap data;
    auto got = Run<PackedVec3>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(PackedVec3Test, ReadViaVectorPtrViaLet) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let p0 = &(P.v);
  let p1 = &(*(p0));
  let x = *p1;
}
)";

    auto* expect = R"(
struct S {
  @internal(packed_vector)
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let p0 = &(P.v);
  let p1 = &(*(p0));
  let x = vec3<f32>(*(p1));
}
)";

    DataMap data;
    auto got = Run<PackedVec3>(src, data);

    EXPECT_EQ(expect, str(got));
}

///

TEST_F(PackedVec3Test, ReadUnaryOp) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = -P.v;
}
)";

    auto* expect = R"(
struct S {
  @internal(packed_vector)
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = -(vec3<f32>(P.v));
}
)";

    DataMap data;
    auto got = Run<PackedVec3>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(PackedVec3Test, ReadBinaryOp) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = P.v + P.v;
}
)";

    auto* expect = R"(
struct S {
  @internal(packed_vector)
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage> P : S;

fn f() {
  let x = (vec3<f32>(P.v) + vec3<f32>(P.v));
}
)";

    DataMap data;
    auto got = Run<PackedVec3>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(PackedVec3Test, WriteVector) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage, read_write> P : S;

fn f() {
  P.v = vec3(1.23);
}
)";

    auto* expect = R"(
struct S {
  @internal(packed_vector)
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage, read_write> P : S;

fn f() {
  P.v = vec3(1.23);
}
)";

    DataMap data;
    auto got = Run<PackedVec3>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(PackedVec3Test, WriteMemberAccess) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage, read_write> P : S;

fn f() {
  P.v.y = 1.23;
}
)";

    auto* expect = R"(
struct S {
  @internal(packed_vector)
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage, read_write> P : S;

fn f() {
  P.v.y = 1.23;
}
)";

    DataMap data;
    auto got = Run<PackedVec3>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(PackedVec3Test, WriteIndexAccessor) {
    auto* src = R"(
struct S {
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage, read_write> P : S;

fn f() {
  P.v[1] = 1.23;
}
)";

    auto* expect = R"(
struct S {
  @internal(packed_vector)
  v : vec3<f32>,
}

@group(0) @binding(0) var<storage, read_write> P : S;

fn f() {
  P.v[1] = 1.23;
}
)";

    DataMap data;
    auto got = Run<PackedVec3>(src, data);

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::transform
