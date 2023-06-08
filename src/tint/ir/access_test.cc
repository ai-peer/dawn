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

#include "src/tint/ir/access.h"

#include "gmock/gmock.h"
#include "gtest/gtest-spi.h"
#include "src/tint/ir/ir_test_helper.h"
#include "src/tint/type/array.h"
#include "src/tint/type/matrix.h"
#include "src/tint/type/struct.h"
#include "src/tint/type/vector.h"

namespace tint::ir {
namespace {

using IR_AccessTest = IRTestHelper;

TEST_F(IR_AccessTest, SetsUsage) {
    auto* ty = mod.Types().pointer(mod.Types().i32(), builtin::AddressSpace::kFunction,
                                   builtin::Access::kReadWrite);
    auto* var = b.Var(ty);
    auto* idx = b.Constant(u32(1));
    auto* a = b.Access(mod.Types().i32(), var, utils::Vector{idx});

    EXPECT_THAT(var->Usages(), testing::UnorderedElementsAre(Usage{a, 0u}));
    EXPECT_THAT(idx->Usages(), testing::UnorderedElementsAre(Usage{a, 1u}));
}

TEST_F(IR_AccessTest, GetSourceObjectTypes_Value) {
    auto* mat = ty.mat4x4(ty.f32());
    auto* arr = ty.array(mat, 1024u);
    auto* str = ty.Get<type::Struct>(
        mod.symbols.Register("MyStruct"),
        utils::Vector{
            ty.Get<type::StructMember>(mod.symbols.Register("a"), ty.f32(), 0u, 0u, 4u, 4u,
                                       type::StructMemberAttributes{}),
            ty.Get<type::StructMember>(mod.symbols.Register("b"), arr, 1u, 256u, 16u, 256u,
                                       type::StructMemberAttributes{}),
        },
        16u, 512u, 512u);
    auto* val = b.FunctionParam(str);
    auto* idx = b.Constant(u32(1));
    auto* a = b.Access(ty.f32(), val, utils::Vector{idx, idx, idx, idx});

    EXPECT_THAT(a->SourceObjectTypes(ty), testing::ElementsAre(str, arr, mat, mat->ColumnType()));
}

TEST_F(IR_AccessTest, GetSourceObjectTypes_Pointer) {
    auto* mat = ty.mat4x4(ty.f32());
    auto* arr = ty.array(mat, 1024u);
    auto* str = ty.Get<type::Struct>(
        mod.symbols.Register("MyStruct"),
        utils::Vector{
            ty.Get<type::StructMember>(mod.symbols.Register("a"), ty.f32(), 0u, 0u, 4u, 4u,
                                       type::StructMemberAttributes{}),
            ty.Get<type::StructMember>(mod.symbols.Register("b"), arr, 1u, 256u, 16u, 256u,
                                       type::StructMemberAttributes{}),
        },
        16u, 512u, 512u);
    auto ptr = [&](auto* el) {
        return ty.pointer(el, builtin::AddressSpace::kFunction, builtin::Access::kReadWrite);
    };
    auto* var = b.Var(ptr(str));
    auto* idx = b.Constant(u32(1));
    auto* a = b.Access(ty.f32(), var, utils::Vector{idx, idx, idx, idx});

    EXPECT_THAT(a->SourceObjectTypes(ty),
                testing::ElementsAre(ptr(str), ptr(arr), ptr(mat), ptr(mat->ColumnType())));
}

TEST_F(IR_AccessTest, Fail_NullType) {
    EXPECT_FATAL_FAILURE(
        {
            Module mod;
            Builder b{mod};
            auto* ty = mod.Types().pointer(mod.Types().i32(), builtin::AddressSpace::kFunction,
                                           builtin::Access::kReadWrite);
            auto* var = b.Var(ty);
            b.Access(nullptr, var, utils::Vector{b.Constant(u32(1))});
        },
        "");
}

TEST_F(IR_AccessTest, Fail_NullObject) {
    EXPECT_FATAL_FAILURE(
        {
            Module mod;
            Builder b{mod};
            b.Access(mod.Types().i32(), nullptr, utils::Vector{b.Constant(u32(1))});
        },
        "");
}

TEST_F(IR_AccessTest, Fail_EmptyIndices) {
    EXPECT_FATAL_FAILURE(
        {
            Module mod;
            Builder b{mod};
            auto* ty = mod.Types().pointer(mod.Types().i32(), builtin::AddressSpace::kFunction,
                                           builtin::Access::kReadWrite);
            auto* var = b.Var(ty);
            b.Access(mod.Types().i32(), var, utils::Empty);
        },
        "");
}

TEST_F(IR_AccessTest, Fail_NullIndex) {
    EXPECT_FATAL_FAILURE(
        {
            Module mod;
            Builder b{mod};
            auto* ty = mod.Types().pointer(mod.Types().i32(), builtin::AddressSpace::kFunction,
                                           builtin::Access::kReadWrite);
            auto* var = b.Var(ty);
            b.Access(mod.Types().i32(), var, utils::Vector<Value*, 1>{nullptr});
        },
        "");
}

}  // namespace
}  // namespace tint::ir
