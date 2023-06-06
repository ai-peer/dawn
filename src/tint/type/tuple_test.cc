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

#include "src/tint/type/tuple.h"
#include "src/tint/type/f32.h"
#include "src/tint/type/i32.h"
#include "src/tint/type/test_helper.h"

namespace tint::type {
namespace {

using I32Test = TestHelper;

TEST_F(I32Test, Creation) {
    auto* a = create<Tuple>(utils::Vector{create<I32>(), create<F32>()});
    auto* b = create<Tuple>(utils::Vector{create<I32>(), create<F32>()});
    EXPECT_EQ(a, b);
}

TEST_F(I32Test, Hash) {
    auto* a = create<Tuple>(utils::Vector{create<I32>(), create<F32>()});
    auto* b = create<Tuple>(utils::Vector{create<I32>(), create<F32>()});
    EXPECT_EQ(a->unique_hash, b->unique_hash);
}

TEST_F(I32Test, Equals) {
    auto* a = create<Tuple>(utils::Vector{create<I32>(), create<F32>()});
    auto* b = create<Tuple>(utils::Vector{create<I32>(), create<F32>()});
    EXPECT_TRUE(a->Equals(*b));
    EXPECT_FALSE(a->Equals(Void{}));
}

TEST_F(I32Test, FriendlyName) {
    auto* t = create<Tuple>(utils::Vector{create<I32>(), create<F32>()});
    EXPECT_EQ(t->FriendlyName(), "[i32, f32]");
}

TEST_F(I32Test, Clone) {
    auto* a = create<Tuple>(utils::Vector{create<I32>(), create<F32>()});

    type::Manager mgr;
    type::CloneContext ctx{{nullptr}, {nullptr, &mgr}};

    auto* t = a->Clone(ctx);
    ASSERT_EQ(t->Types().Length(), 2u);
    ASSERT_TRUE(t->Types()[0]->Is<I32>());
    ASSERT_TRUE(t->Types()[1]->Is<F32>());
}

}  // namespace
}  // namespace tint::type
