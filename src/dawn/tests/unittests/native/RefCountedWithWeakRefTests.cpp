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

#include <atomic>
#include <thread>
#include <utility>

#include "dawn/native/RefCountedWithWeakRef.h"
#include "gtest/gtest.h"

namespace {
class RCTest : public dawn::native::RefCountedWithWeakRef<RCTest> {
  public:
    RCTest() = delete;

    explicit RCTest(std::atomic_bool* deleted) : mDeleted(deleted) {}

    ~RCTest() override { *mDeleted = true; }

  private:
    std::atomic_bool* mDeleted = nullptr;
};

// Test strong ref can be obtained from weak ref.
TEST(RefCountedWithWeakRef, StrongRefFromWeakRef) {
    std::atomic_bool deleted(false);
    auto test = AcquireRef(new RCTest(&deleted));
    EXPECT_EQ(test->GetRefCountForTesting(), 1u);

    auto weakRef = test->GetWeakReference();
    EXPECT_EQ(test->GetRefCountForTesting(), 1u);

    // Get strong ref from weak ref.
    auto strongRef = weakRef->GetStrongReference();
    EXPECT_EQ(strongRef->GetRefCountForTesting(), 2u);

    test = nullptr;
    strongRef = nullptr;

    EXPECT_TRUE(deleted.load());
}

// Test adding weak refs doesn't keep the RC alive.
TEST(RefCountedWithWeakRef, AddingWeakRefDoesntKeepAlive) {
    std::atomic_bool deleted(false);
    auto test = AcquireRef(new RCTest(&deleted));
    EXPECT_EQ(test->GetRefCountForTesting(), 1u);

    auto strongRef1 = test;
    EXPECT_EQ(strongRef1->GetRefCountForTesting(), 2u);

    auto weakRef1 = test->GetWeakReference();
    EXPECT_EQ(test->GetRefCountForTesting(), 2u);

    // Get strong ref from weak ref.
    auto strongRef2 = weakRef1->GetStrongReference();
    EXPECT_EQ(strongRef2->GetRefCountForTesting(), 3u);

    test = nullptr;
    strongRef1 = nullptr;

    // 2nd external weak ref.
    auto weakRef2 = strongRef2->GetWeakReference();
    EXPECT_EQ(strongRef2->GetRefCountForTesting(), 1u);
    // One weak ref is hold by the RC itself.
    EXPECT_EQ(weakRef1->GetRefCountForTesting(), 3u);

    // last strong ref dropped, weak refs shoudn't be able to obtain strong ref anymore.
    strongRef2 = nullptr;
    EXPECT_TRUE(deleted.load());
    EXPECT_EQ(weakRef1->GetStrongReference(), nullptr);
    EXPECT_EQ(weakRef2->GetStrongReference(), nullptr);

    weakRef2 = nullptr;
    EXPECT_EQ(weakRef1->GetRefCountForTesting(), 1u);
}

// Test that Reference and Release atomically change the refcount.
TEST(RefCountedWithWeakRef, RaceOnReferenceRelease) {
    std::atomic_bool deleted(false);
    auto* test = new RCTest(&deleted);

    auto referenceManyTimes = [test]() {
        for (uint32_t i = 0; i < 100000; ++i) {
            test->Reference();
        }
    };
    std::thread t1(referenceManyTimes);
    std::thread t2(referenceManyTimes);

    t1.join();
    t2.join();
    EXPECT_EQ(test->GetRefCountForTesting(), 200001u);

    auto releaseManyTimes = [test]() {
        for (uint32_t i = 0; i < 100000; ++i) {
            test->Release();
        }
    };

    std::thread t3(releaseManyTimes);
    std::thread t4(releaseManyTimes);
    t3.join();
    t4.join();
    EXPECT_EQ(test->GetRefCountForTesting(), 1u);

    test->Release();
    EXPECT_TRUE(deleted.load());
}

// Test that GetStrongReference and Release atomically change the refcount.
TEST(RefCountedWithWeakRef, RaceOnGetStrongReferenceRelease) {
    std::atomic_bool deleted(false);
    std::atomic<uint32_t> numStrongRefsObtained(0);
    std::atomic<uint32_t> numStrongRefsFailed(0);
    auto* test = new RCTest(&deleted);

    auto testWeakRef = test->GetWeakReference();

    std::thread t1([test]() {
        for (uint32_t i = 0; i < 100000; ++i) {
            test->Reference();
        }
    });

    t1.join();
    test->Release();
    EXPECT_EQ(test->GetRefCountForTesting(), 100000u);

    auto releaseManyTimes = [test, testWeakRef, &numStrongRefsObtained, &numStrongRefsFailed]() {
        for (uint32_t i = 0; i < 50000; ++i) {
            test->Release();

            if (testWeakRef->GetStrongReference() != nullptr) {
                numStrongRefsObtained++;
            } else {
                numStrongRefsFailed++;
            }
        }
    };

    std::thread t2(releaseManyTimes);
    std::thread t3(releaseManyTimes);
    t2.join();
    t3.join();

    EXPECT_EQ(numStrongRefsObtained.load(), 100000u - 1);
    EXPECT_EQ(numStrongRefsFailed.load(), 1u);
    EXPECT_EQ(testWeakRef->GetStrongReference(), nullptr);

    EXPECT_TRUE(deleted.load());
}

}  // namespace
