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

#include "dawn/common/RefCounted.h"
#include "gtest/gtest.h"

namespace dawn {
namespace {

class WeakRefCountedBaseT : public WeakRefCounted {};
class WeakRefCountedDerivedT : public WeakRefCountedBaseT {};

TEST(WeakRefTests, Basic) {
    Ref<WeakRefCountedBaseT> base = AcquireRef(new WeakRefCountedBaseT());
    WeakRef<WeakRefCountedBaseT> weak = base.GetWeakRef();
    WeakRef<WeakRefCountedBaseT> weak2 = weak;
    EXPECT_TRUE(weak.IsValid());
    EXPECT_TRUE(weak.Get().Get() == base.Get());
    EXPECT_TRUE(weak2.IsValid());

    base = nullptr;
    EXPECT_FALSE(weak.IsValid());
    EXPECT_TRUE(weak.Get() == nullptr);
}

TEST(WeakRefTests, Derived) {
    Ref<WeakRefCountedDerivedT> base = AcquireRef(new WeakRefCountedDerivedT());
    WeakRef<WeakRefCountedDerivedT> weak = base.GetWeakRef();
    WeakRef<WeakRefCountedBaseT> weak2 = weak;
    EXPECT_TRUE(weak.IsValid());
    EXPECT_TRUE(weak.Get().Get() == base.Get());
    EXPECT_TRUE(weak2.IsValid());

    base = nullptr;
    EXPECT_FALSE(weak.IsValid());
    EXPECT_TRUE(weak.Get() == nullptr);
}

}  // anonymous namespace
}  // namespace dawn
