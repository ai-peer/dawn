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

#include <string>
#include <thread>
#include <utility>

#include "dawn/common/MutexProtected.h"
#include "dawn/common/Ref.h"
#include "dawn/common/RefCounted.h"
#include "gtest/gtest.h"

namespace dawn {
namespace {

using ::testing::Test;
using ::testing::Types;

// Simple thread-unsafe counter class.
class CounterT : public RefCounted {
  public:
    CounterT() = default;
    explicit CounterT(int count) : mCount(count) {}

    int Get() const { return mCount; }

    void Increment() { mCount++; }
    void Decrement() { mCount--; }

  private:
    int mCount = 0;
};

template <typename T>
MutexProtected<T> CreateDefault() {
    if constexpr (IsRef<T>::value) {
        return MutexProtected<T>(AcquireRef(new typename UnwrapRef<T>::type()));
    } else {
        return MutexProtected<T>();
    }
}

template <typename T, typename... Args>
MutexProtected<T> CreateCustom(Args&&... args) {
    if constexpr (IsRef<T>::value) {
        return MutexProtected<T>(
            AcquireRef(new typename UnwrapRef<T>::type(std::forward<Args>(args)...)));
    } else {
        return MutexProtected<T>(std::forward<Args>(args)...);
    }
}

template <typename T>
class MutexProtectedTest : public Test {};

class MutexProtectedTestTypeNames {
  public:
    template <typename T>
    static std::string GetName(int) {
        if (std::is_same<T, CounterT>()) {
            return "CounterT";
        }
        if (std::is_same<T, Ref<CounterT>>()) {
            return "Ref<CounterT>";
        }
    }
};
using MutexProtectedTestTypes = Types<CounterT, Ref<CounterT>>;
TYPED_TEST_SUITE(MutexProtectedTest, MutexProtectedTestTypes, MutexProtectedTestTypeNames);

TYPED_TEST(MutexProtectedTest, DefaultCtor) {
    static constexpr int kIncrementCount = 100;
    static constexpr int kDecrementCount = 50;

    MutexProtected<TypeParam> counter = CreateDefault<TypeParam>();

    auto increment = [&] {
        for (uint32_t i = 0; i < kIncrementCount; i++) {
            counter->Increment();
        }
    };
    auto useIncrement = [&] {
        for (uint32_t i = 0; i < kIncrementCount; i++) {
            UseProtected([](auto c) { c->Increment(); }, counter);
        }
    };
    auto decrement = [&] {
        for (uint32_t i = 0; i < kDecrementCount; i++) {
            counter->Decrement();
        }
    };
    auto useDecrement = [&] {
        for (uint32_t i = 0; i < kDecrementCount; i++) {
            UseProtected([](auto c) { c->Decrement(); }, counter);
        }
    };

    std::thread incrementThread(increment);
    std::thread useIncrementThread(useIncrement);
    std::thread decrementThread(decrement);
    std::thread useDecrementThread(useDecrement);
    incrementThread.join();
    useIncrementThread.join();
    decrementThread.join();
    useDecrementThread.join();

    EXPECT_EQ(counter->Get(), 2 * (kIncrementCount - kDecrementCount));
}

TYPED_TEST(MutexProtectedTest, CustomCtor) {
    static constexpr int kIncrementCount = 100;
    static constexpr int kDecrementCount = 50;
    static constexpr int kStartingcount = -100;

    MutexProtected<TypeParam> counter = CreateCustom<TypeParam>(kStartingcount);

    auto increment = [&] {
        for (uint32_t i = 0; i < kIncrementCount; i++) {
            counter->Increment();
        }
    };
    auto useIncrement = [&] {
        for (uint32_t i = 0; i < kIncrementCount; i++) {
            UseProtected([](auto c) { c->Increment(); }, counter);
        }
    };
    auto decrement = [&] {
        for (uint32_t i = 0; i < kDecrementCount; i++) {
            counter->Decrement();
        }
    };
    auto useDecrement = [&] {
        for (uint32_t i = 0; i < kDecrementCount; i++) {
            UseProtected([](auto c) { c->Decrement(); }, counter);
        }
    };

    std::thread incrementThread(increment);
    std::thread useIncrementThread(useIncrement);
    std::thread decrementThread(decrement);
    std::thread useDecrementThread(useDecrement);
    incrementThread.join();
    useIncrementThread.join();
    decrementThread.join();
    useDecrementThread.join();

    EXPECT_EQ(counter->Get(), kStartingcount + 2 * (kIncrementCount - kDecrementCount));
}

TYPED_TEST(MutexProtectedTest, MultipleProtected) {
    static constexpr int kIncrementCount = 100;

    MutexProtected<TypeParam> c1 = CreateDefault<TypeParam>();
    MutexProtected<TypeParam> c2 = CreateDefault<TypeParam>();

    auto increment = [&] {
        for (uint32_t i = 0; i < kIncrementCount; i++) {
            UseProtected(
                [](auto x1, auto x2) {
                    x1->Increment();
                    x2->Increment();
                },
                c1, c2);
        }
    };
    auto validate = [&] {
        for (uint32_t i = 0; i < kIncrementCount; i++) {
            UseProtected([](auto x1, auto x2) { EXPECT_EQ(x1->Get(), x2->Get()); }, c1, c2);
        }
    };
    std::thread incrementThread(increment);
    std::thread validateThread(validate);
    incrementThread.join();
    validateThread.join();
}

TYPED_TEST(MutexProtectedTest, RecursiveProtected) {
    static constexpr int kIncrementCount = 100;

    MutexProtected<TypeParam> c1 = CreateDefault<TypeParam>();
    MutexProtected<TypeParam> c2 = CreateDefault<TypeParam>();

    auto increment = [&] {
        for (uint32_t i = 0; i < kIncrementCount; i++) {
            UseProtected(
                [&c2](auto x1) {
                    UseProtected(
                        [&x1](auto x2) {
                            x1->Increment();
                            x2->Increment();
                        },
                        c2);
                },
                c1);
        }
    };
    auto validate = [&] {
        for (uint32_t i = 0; i < kIncrementCount; i++) {
            UseProtected([](auto c1, auto c2) { EXPECT_EQ(c1->Get(), c2->Get()); }, c1, c2);
        }
    };
    std::thread incrementThread(increment);
    std::thread validateThread(validate);
    incrementThread.join();
    validateThread.join();
}

}  // anonymous namespace
}  // namespace dawn
