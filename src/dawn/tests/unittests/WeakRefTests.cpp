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

#include <experimental/type_traits>
#include <functional>
#include <thread>

#include "dawn/common/WeakRef.h"
#include "dawn/common/WeakRefCounted.h"
#include "dawn/utils/Signal.h"
#include "gtest/gtest.h"

namespace dawn {
namespace {

using utils::Signal;

class RefCountedT : public RefCounted {};

class WeakRefCountedBaseA : public RefCounted, public WeakRefCounted<WeakRefCountedBaseA> {
  public:
    WeakRefCountedBaseA() = default;
    explicit WeakRefCountedBaseA(std::function<void(WeakRefCountedBaseA*)> deleteFn)
        : mDeleteFn(deleteFn) {}

  protected:
    ~WeakRefCountedBaseA() override { mDeleteFn(this); }

  private:
    std::function<void(WeakRefCountedBaseA*)> mDeleteFn = [](WeakRefCountedBaseA*) -> void {};
};

class WeakRefCountedDerivedA : public WeakRefCountedBaseA {
  public:
    WeakRefCountedDerivedA() = default;
    explicit WeakRefCountedDerivedA(std::function<void(WeakRefCountedBaseA*)> deleteFn)
        : WeakRefCountedBaseA(deleteFn) {}
};

class WeakRefCountedBaseB : public RefCounted, public WeakRefCounted<WeakRefCountedBaseB> {};
class WeakRefCountedDerivedB : public WeakRefCountedBaseB {};

// When the original refcounted object is destroyed, all WeakRefs are no longer able to Promote.
TEST(WeakRefTests, BasicPromote) {
    Ref<WeakRefCountedBaseA> base = AcquireRef(new WeakRefCountedBaseA());
    WeakRef<WeakRefCountedBaseA> weak = base.GetWeakRef();
    EXPECT_EQ(weak.Promote().Get(), base.Get());

    base = nullptr;
    EXPECT_EQ(weak.Promote(), nullptr);
}

// When the original refcounted object is destroyed, all WeakRefs, including upcasted ones, are no
// longer able to Promote.
TEST(WeakRefTests, DerivedPromote) {
    Ref<WeakRefCountedDerivedA> base = AcquireRef(new WeakRefCountedDerivedA());
    WeakRef<WeakRefCountedDerivedA> weak1 = base.GetWeakRef();
    WeakRef<WeakRefCountedBaseA> weak2 = weak1;
    WeakRef<WeakRefCountedBaseA> weak3 = base.GetWeakRef();
    EXPECT_TRUE(weak1.Promote().Get() == base.Get());
    EXPECT_TRUE(weak2.Promote().Get() == base.Get());
    EXPECT_TRUE(weak3.Promote().Get() == base.Get());

    base = nullptr;
    EXPECT_TRUE(weak1.Promote() == nullptr);
    EXPECT_TRUE(weak2.Promote() == nullptr);
    EXPECT_TRUE(weak3.Promote() == nullptr);
}

// Trying to promote a WeakRef to a Ref while the original value is being destroyed returns nullptr.
TEST(WeakRefTests, DeletingAndPromoting) {
    Signal signalA, signalB;
    Ref<WeakRefCountedBaseA> base = AcquireRef(new WeakRefCountedBaseA([&](WeakRefCountedBaseA*) {
        signalB.Fire();
        signalA.Wait();
    }));

    auto f = [&] {
        WeakRef<WeakRefCountedBaseA> weak = base.GetWeakRef();
        signalA.Fire();
        signalB.Wait();
        EXPECT_EQ(weak.Promote().Get(), nullptr);
        signalA.Fire();
    };
    std::thread t(f);

    signalA.Wait();
    base = nullptr;
    t.join();
}

// Helper detection utilities for verifying that unintended assignments are not allowed.
template <typename L, typename R>
using weakref_copyable_t =
    decltype(std::declval<WeakRef<L>&>() = std::declval<const WeakRef<R>&>());
template <typename L, typename R>
using weakref_movable_t =
    decltype(std::declval<WeakRef<L>&>() = std::declval<const WeakRef<R>&&>());
TEST(WeakRefTests, CrossTypesAssignments) {
    // Same type and upcasting is allowed.
    static_assert(std::experimental::is_detected_v<weakref_copyable_t, WeakRefCountedBaseA,
                                                   WeakRefCountedBaseA>,
                  "Same type copy assignment is allowed.");
    static_assert(std::experimental::is_detected_v<weakref_movable_t, WeakRefCountedBaseA,
                                                   WeakRefCountedBaseA>,
                  "Same type move assignment is allowed.");

    static_assert(std::experimental::is_detected_v<weakref_copyable_t, WeakRefCountedBaseA,
                                                   WeakRefCountedDerivedA>,
                  "Upcasting type copy assignment is allowed.");
    static_assert(std::experimental::is_detected_v<weakref_movable_t, WeakRefCountedBaseA,
                                                   WeakRefCountedDerivedA>,
                  "Upcasting type move assignment is allowed.");

    // Same type, but down casting is not allowed.
    static_assert(!std::experimental::is_detected_v<weakref_copyable_t, WeakRefCountedDerivedA,
                                                    WeakRefCountedBaseA>,
                  "Downcasting type copy assignment is not allowed.");
    static_assert(!std::experimental::is_detected_v<weakref_movable_t, WeakRefCountedDerivedA,
                                                    WeakRefCountedBaseA>,
                  "Downcasting type move assignment is not allowed.");

    // Cross types are not allowed.
    static_assert(!std::experimental::is_detected_v<weakref_copyable_t, WeakRefCountedBaseA,
                                                    WeakRefCountedBaseB>,
                  "Cross type copy assignment is not allowed.");
    static_assert(!std::experimental::is_detected_v<weakref_movable_t, WeakRefCountedBaseA,
                                                    WeakRefCountedBaseB>,
                  "Cross type move assignment is not allowed.");
    static_assert(!std::experimental::is_detected_v<weakref_copyable_t, WeakRefCountedBaseA,
                                                    WeakRefCountedDerivedB>,
                  "Cross type upcasting copy assignment is not allowed.");
    static_assert(!std::experimental::is_detected_v<weakref_movable_t, WeakRefCountedBaseA,
                                                    WeakRefCountedDerivedB>,
                  "Cross type upcasting move assignment is not allowed.");
}

// Helper detection utilty to verify whether GetWeakRef is enabled.
template <typename T>
using can_get_weakref_t = decltype(std::declval<Ref<T>>().GetWeakRef());
TEST(WeakRefTests, GetWeakRef) {
    // The GetWeakRef function is only available on types that extend WeakRefCounted.
    static_assert(std::experimental::is_detected_v<can_get_weakref_t, WeakRefCountedBaseA>,
                  "GetWeakRef is enabled on classes that directly extend WeakRefCounted.");
    static_assert(std::experimental::is_detected_v<can_get_weakref_t, WeakRefCountedDerivedA>,
                  "GetWeakRef is enabled on classes that indirectly extend WeakRefCounted.");

    static_assert(!std::experimental::is_detected_v<can_get_weakref_t, RefCountedT>,
                  "GetWeakRef is disabled on classes that do not extend WeakRefCounted.");
}

}  // anonymous namespace
}  // namespace dawn
