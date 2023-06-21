// Copyright 2023 The Dawn Authors
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

#include "dawn/common/WeakRef.h"
#include "dawn/common/WeakRefCounted.h"
#include "gtest/gtest.h"

namespace dawn {
namespace {

class WeakRefCountedBaseA : public RefCounted, public WeakRefCounted<WeakRefCountedBaseA> {};
class WeakRefCountedDerivedA : public WeakRefCountedBaseA {};

class WeakRefCountedBaseB : public RefCounted, public WeakRefCounted<WeakRefCountedBaseB> {};
class WeakRefCountedDerivedB : public WeakRefCountedBaseB {};

TEST(WeakRefTests, Basic) {
    Ref<WeakRefCountedBaseA> base = AcquireRef(new WeakRefCountedBaseA());
    // WeakRef<WeakRefCountedBaseA> weak = GetWeakRef<WeakRefCountedBaseA>(base);
    WeakRef<WeakRefCountedBaseA> weak = base.GetWeakRef();

    // Breaks compilation:
    // WeakRef<WeakRefCountedBaseB> weak2 = GetWeakRef<WeakRefCountedBaseA>(base);
    // EXPECT_TRUE(weak.IsValid());
    EXPECT_EQ(weak.Promote().Get(), base.Get());

    base = nullptr;
    EXPECT_EQ(weak.Promote(), nullptr);
}

TEST(WeakRefTests, Derived) {
    Ref<WeakRefCountedDerivedA> base = AcquireRef(new WeakRefCountedDerivedA());
    WeakRef<WeakRefCountedDerivedA> weak = base.GetWeakRef();
    WeakRef<WeakRefCountedBaseA> weak2 = weak;
    WeakRef<WeakRefCountedBaseA> weak3 = base.GetWeakRef();
    EXPECT_TRUE(weak.Promote().Get() == base.Get());
    EXPECT_TRUE(weak2.Promote().Get() == base.Get());
    EXPECT_TRUE(weak3.Promote().Get() == base.Get());

    base = nullptr;
    EXPECT_TRUE(weak.Promote() == nullptr);
    EXPECT_TRUE(weak2.Promote() == nullptr);
    EXPECT_TRUE(weak3.Promote() == nullptr);
}

}  // anonymous namespace
}  // namespace dawn
