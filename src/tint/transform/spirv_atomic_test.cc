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

#include "src/tint/transform/spirv_atomic.h"

#include "src/tint/reader/wgsl/parser_impl.h"
#include "src/tint/transform/test_helper.h"

namespace tint::transform {
namespace {

class SpirvAtomicTest : public TransformTest {
  public:
    Output Run(std::string in) {
        auto file = std::make_unique<Source::File>("test", in);
        auto parser = reader::wgsl::ParserImpl(file.get());
        parser.Parse();

        auto& b = parser.builder();

        {
            b.Func(
                "stub_atomic_store_u32", {b.Param("p0", b.ty.u32()), b.Param("p1", b.ty.u32())},
                b.ty.void_(), {},
                {b.ASTNodes().Create<SpirvAtomic::Stub>(b.ID(), sem::BuiltinType::kAtomicStore)});
        }
        {
            b.Func(
                "stub_atomic_store_i32", {b.Param("p0", b.ty.i32()), b.Param("p1", b.ty.i32())},
                b.ty.void_(), {},
                {b.ASTNodes().Create<SpirvAtomic::Stub>(b.ID(), sem::BuiltinType::kAtomicStore)});
        }

        // Keep this pointer alive after Transform() returns
        files_.emplace_back(std::move(file));

        return TransformTest::Run<SpirvAtomic>(Program(std::move(b)));
    }

  private:
    std::vector<std::unique_ptr<Source::File>> files_;
};

TEST_F(SpirvAtomicTest, ArrayOfU32) {
    auto* src = R"(
var<workgroup> wg : array<u32, 4>;

fn f() {
  stub_atomic_store_u32(wg[1], 1u);
}
)";

    auto* expect = R"(
var<workgroup> wg : array<atomic<u32>, 4u>;

fn f() {
  atomicStore(&(wg[1]), 1u);
}
)";

    auto got = Run(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(SpirvAtomicTest, ArraysOfU32) {
    auto* src = R"(
var<workgroup> wg : array<array<array<u32, 1>, 2>, 3>;

fn f() {
  stub_atomic_store_u32(wg[2][1][0], 1u);
}
)";

    auto* expect = R"(
var<workgroup> wg : array<array<array<atomic<u32>, 1u>, 2u>, 3u>;

fn f() {
  atomicStore(&(wg[2][1][0]), 1u);
}
)";

    auto got = Run(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(SpirvAtomicTest, AliasedArraysOfU32) {
    auto* src = R"(
type A0 = u32;

type A1 = array<A0, 1>;

type A2 = array<A1, 2>;

type A3 = array<A2, 3>;

var<workgroup> wg : A3;

fn f() {
  stub_atomic_store_u32(wg[2][1][0], 1u);
}
)";

    auto* expect = R"(
type A0 = u32;

type A1 = array<A0, 1>;

type A2 = array<A1, 2>;

type A3 = array<A2, 3>;

var<workgroup> wg : array<array<array<atomic<u32>, 1u>, 2u>, 3u>;

fn f() {
  atomicStore(&(wg[2][1][0]), 1u);
}
)";

    auto got = Run(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(SpirvAtomicTest, FlatStructSingleAtomic) {
    auto* src = R"(
struct S {
  a : u32,
}

var<workgroup> wg : S;

fn f() {
  stub_atomic_store_u32(wg.a, 1u);
}
)";

    auto* expect = R"(
struct S_atomic {
  a : atomic<u32>,
}

struct S {
  a : u32,
}

var<workgroup> wg : S_atomic;

fn f() {
  atomicStore(&(wg.a), 1u);
}
)";

    auto got = Run(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(SpirvAtomicTest, FlatStructMultipleAtomic) {
    auto* src = R"(
struct S {
  a : u32,
  b : i32,
}

var<workgroup> wg : S;

fn f1() {
  stub_atomic_store_u32(wg.a, 1u);
}

fn f2() {
  stub_atomic_store_i32(wg.b, 2i);
}
)";

    auto* expect = R"(
struct S_atomic {
  a : atomic<u32>,
  b : atomic<i32>,
}

struct S {
  a : u32,
  b : i32,
}

var<workgroup> wg : S_atomic;

fn f1() {
  atomicStore(&(wg.a), 1u);
}

fn f2() {
  atomicStore(&(wg.b), 2i);
}
)";

    auto got = Run(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(SpirvAtomicTest, NestedStruct) {
    auto* src = R"(
struct S0 {
  a : u32,
  b : i32,
  c : u32,
}

struct S1 {
  a : i32,
  b : u32,
  c : S0,
}

struct S2 {
  a : i32,
  b : S1,
  c : u32,
}

var<workgroup> wg : S2;

fn f() {
  stub_atomic_store_u32(wg.b.c.a, 1u);
}
)";

    auto* expect = R"(
struct S0_atomic {
  a : atomic<u32>,
  b : i32,
  c : u32,
}

struct S0 {
  a : u32,
  b : i32,
  c : u32,
}

struct S1_atomic {
  a : i32,
  b : u32,
  c : S0_atomic,
}

struct S1 {
  a : i32,
  b : u32,
  c : S0,
}

struct S2_atomic {
  a : i32,
  b : S1_atomic,
  c : u32,
}

struct S2 {
  a : i32,
  b : S1,
  c : u32,
}

var<workgroup> wg : S2_atomic;

fn f() {
  atomicStore(&(wg.b.c.a), 1u);
}
)";

    auto got = Run(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(SpirvAtomicTest, ArrayOfStruct) {
    auto* src = R"(
struct S {
  a : u32,
  b : i32,
  c : u32,
}

@group(0) @binding(1) var<storage, read_write> arr : array<S>;

fn f() {
  stub_atomic_store_i32(arr[4].b, 1i);
}
)";

    auto* expect = R"(
struct S_atomic {
  a : u32,
  b : atomic<i32>,
  c : u32,
}

struct S {
  a : u32,
  b : i32,
  c : u32,
}

@group(0) @binding(1) var<storage, read_write> arr : array<S_atomic>;

fn f() {
  atomicStore(&(arr[4].b), 1i);
}
)";

    auto got = Run(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(SpirvAtomicTest, StructOfArray) {
    auto* src = R"(
struct S {
  a : array<i32>,
}

@group(0) @binding(1) var<storage, read_write> s : S;

fn f() {
  stub_atomic_store_i32(s.a[4], 1i);
}
)";

    auto* expect = R"(
struct S_atomic {
  a : array<atomic<i32>>,
}

struct S {
  a : array<i32>,
}

@group(0) @binding(1) var<storage, read_write> s : S_atomic;

fn f() {
  atomicStore(&(s.a[4]), 1i);
}
)";

    auto got = Run(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(SpirvAtomicTest, ViaPtrLet) {
    auto* src = R"(
struct S {
  i : i32,
}

@group(0) @binding(1) var<storage, read_write> s : S;

fn f() {
  let p0 = &(s);
  let p1 : ptr<storage, i32, read_write> = &((*(p0)).i);
  stub_atomic_store_i32(*p1, 1i);
}
)";

    auto* expect =
        R"(
struct S_atomic {
  i : atomic<i32>,
}

struct S {
  i : i32,
}

@group(0) @binding(1) var<storage, read_write> s : S_atomic;

fn f() {
  let p0 = &(s);
  let p1 : ptr<storage, atomic<i32>, read_write> = &((*(p0)).i);
  atomicStore(&(*(p1)), 1i);
}
)";

    auto got = Run(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(SpirvAtomicTest, StructIsolatedMixedUsage) {
    auto* src = R"(
struct S {
  i : i32,
}

@group(0) @binding(1) var<storage, read_write> s : S;

fn f() {
  stub_atomic_store_i32(s.i, 1i);
}

fn another_usage() {
  var s : S;
  let x : i32 = s.i;
  s.i = 3i;
}
)";

    auto* expect =
        R"(
struct S_atomic {
  i : atomic<i32>,
}

struct S {
  i : i32,
}

@group(0) @binding(1) var<storage, read_write> s : S_atomic;

fn f() {
  atomicStore(&(s.i), 1i);
}

fn another_usage() {
  var s : S;
  let x : i32 = s.i;
  s.i = 3i;
}
)";

    auto got = Run(src);

    EXPECT_EQ(expect, str(got));
}

// This sort of mixed usage isn't handled yet. Not sure if we need to just yet.
// If we don't, then the transform should give sensible diagnostics instead of producing invalid
// WGSL.
TEST_F(SpirvAtomicTest, DISABLED_StructComplexMixedUsage) {
    auto* src = R"(
struct S {
  i : i32,
}

@group(0) @binding(1) var<storage, read_write> s : S;

fn f() {
  let x : i32 = s.i;
  stub_atomic_store_i32(s.i, 1i);
  s.i = 3i;
}
)";

    auto* expect =
        R"(
struct S_atomic {
  i : atomic<i32>,
}

struct S {
  i : i32,
}

@group(0) @binding(1) var<storage, read_write> s : S_atomic;

fn f() {
  let x : i32 = atomicLoad(&s.i);
  stub_atomic_store_i32(s.i, 1i);
  atomicStore(&(s.i), 1i);
}
)";

    auto got = Run(src);

    EXPECT_EQ(expect, str(got));
}
}  // namespace
}  // namespace tint::transform
