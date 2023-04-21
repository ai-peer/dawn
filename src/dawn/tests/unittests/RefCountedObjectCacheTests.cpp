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
#include <functional>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "dawn/common/RefCountedObjectCache.h"
#include "dawn/utils/SystemUtils.h"
#include "dawn/utils/TestUtils.h"
#include "gtest/gtest.h"

namespace {
class Object : public RefCounted {
  public:
    Object() = default;
    explicit Object(size_t key) : mKey(key) {}

    size_t GetKey() const { return mKey; }

  private:
    const size_t mKey = 0;
};

struct HashFunc {
    size_t operator()(const Object* obj) const { return obj->GetKey(); }
};

struct EqualKey {
    bool operator()(const Object* lhs, const Object* rhs) const {
        return lhs->GetKey() == rhs->GetKey();
    }
};

using Cache = RefCountedObjectCache<Object, HashFunc, EqualKey>;

// class auto erase itself from the cache when being deleted.
class AutoRemoveObject : public Object {
  public:
    AutoRemoveObject() = delete;
    AutoRemoveObject(Cache& cache, size_t key) : Object(key), mCache(cache) {}

  private:
    void DeleteThis() override { mCache.Erase(this); }

    Cache& mCache;
};

class RefCountedObjectCacheTests : public ::testing::Test {
  protected:
    Cache cache;
};

TEST_F(RefCountedObjectCacheTests, StartEmpty) {
    EXPECT_TRUE(cache.IsEmpty());
}

// Test that Insert doesn't retain any ref.
TEST_F(RefCountedObjectCacheTests, SimpleInsertNoRetain) {
    auto object = AcquireRef(new Object);

    auto [cached, inserted] = cache.Insert(object.Get());

    EXPECT_TRUE(inserted);
    EXPECT_FALSE(cache.IsEmpty());
    EXPECT_EQ(object.Get(), cached.Get());
    cached = nullptr;

    EXPECT_EQ(object.Get()->GetRefCountForTesting(), 1u);
}

// Test that Erase() existing object works
TEST_F(RefCountedObjectCacheTests, EraseExisting) {
    auto object = AcquireRef(new Object);

    auto [cached, inserted] = cache.Insert(object.Get());
    EXPECT_TRUE(inserted);
    EXPECT_FALSE(cache.IsEmpty());

    Ref<Object> foundRef = cache.Find(object.Get());
    EXPECT_EQ(object.Get(), foundRef.Get());

    cache.Erase(object.Get());
    EXPECT_TRUE(cache.IsEmpty());

    foundRef = cache.Find(object.Get());
    EXPECT_EQ(nullptr, foundRef.Get());
}

// Test that Erase() non-existing object should do nothing
TEST_F(RefCountedObjectCacheTests, EraseNonExisting) {
    auto object1 = AcquireRef(new Object(1));
    auto object2 = AcquireRef(new Object(2));

    auto [cached1, inserted1] = cache.Insert(object1.Get());
    EXPECT_TRUE(inserted1);

    cache.Erase(object2.Get());
    EXPECT_FALSE(cache.IsEmpty());

    Ref<Object> foundRef = cache.Find(object1.Get());
    EXPECT_EQ(object1.Get(), foundRef.Get());
}

// Test that Erase() using "similar" object should erase the stored object.
// "similar" object is object having the same key.
TEST_F(RefCountedObjectCacheTests, EraseSimilarObject) {
    auto object1 = AcquireRef(new Object(1));
    auto object2 = AcquireRef(new Object(1));

    auto [cached1, inserted1] = cache.Insert(object1.Get());
    EXPECT_TRUE(inserted1);

    cache.Erase(object2.Get());
    EXPECT_TRUE(cache.IsEmpty());

    Ref<Object> foundRef = cache.Find(object1.Get());
    EXPECT_EQ(nullptr, foundRef.Get());
}

// Test the inserting different objects work
TEST_F(RefCountedObjectCacheTests, InsertDifferentObjects) {
    auto object1 = AcquireRef(new Object(1));
    auto object2 = AcquireRef(new Object(2));

    {
        auto [cached1, inserted1] = cache.Insert(object1.Get());
        EXPECT_TRUE(inserted1);

        auto [cached2, inserted2] = cache.Insert(object2.Get());
        EXPECT_TRUE(inserted2);
    }

    Ref<Object> cachedRef1 = cache.Find(object1.Get());
    EXPECT_EQ(object1.Get(), cachedRef1.Get());

    Ref<Object> cachedRef2 = cache.Find(object2.Get());
    EXPECT_EQ(object2.Get(), cachedRef2.Get());
}

// Test that insert duplication should return cached object & false.
TEST_F(RefCountedObjectCacheTests, InsertDuplication) {
    auto object1 = AcquireRef(new Object(1));
    auto object2 = AcquireRef(new Object(1));

    auto [cached1, inserted1] = cache.Insert(object1.Get());
    EXPECT_TRUE(inserted1);
    EXPECT_EQ(cached1.Get(), object1.Get());

    auto [cached2, inserted2] = cache.Insert(object2.Get());
    EXPECT_FALSE(inserted2);
    EXPECT_EQ(cached2.Get(), object1.Get());
    EXPECT_NE(cached2.Get(), object2.Get());
}

// Test the racing between duplicated object insertion and deletion on multiple thread
TEST_F(RefCountedObjectCacheTests, RaceInsertAndDeleteDuplications) {
    // Repeat creating 100 threads 100 times. On some systems, it is not allowed to create large
    // number of threads so we have to create small number of threads repeatedly.
    for (int repeat = 0; repeat < 100; ++repeat) {
        utils::RunInParallel(100, [this](uint32_t) {
            auto object = new AutoRemoveObject(cache, 1);

            cache.Insert(object);

            auto cachedRef = cache.Find(object);
            if (cachedRef != nullptr) {
                // cachedRef can be nullptr if object is removed on another thread.
                EXPECT_EQ(cachedRef->GetKey(), object->GetKey());
            }

            object->Release();
        });

        EXPECT_TRUE(cache.IsEmpty());
    }
}
}  // namespace
