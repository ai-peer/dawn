// Copyright 2019 The Dawn Authors
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

#include <gtest/gtest.h>

#include "dawn_native/d3d12/ResidencyManagerD3D12.h"
#include "dawn_native/d3d12/d3d12_platform.h"

#include <vector>

using namespace dawn_native::d3d12;

class LRUTests : public testing::Test {
  public:
    uint32_t kNumElements = 100;
};

TEST_F(LRUTests, InsertAndEvict) {
    LRUCache lruCache;
    std::vector<LRUEntry*> insertedEntries;
    for (uint32_t i = 0; i < kNumElements; i++) {
        // Create an LRUEntry
        LRUEntry* entry = new LRUEntry(nullptr, 1);
        // Save the LRUEntry's address
        insertedEntries.push_back(entry);
        // Insert LRUEntry into LRUCache
        lruCache.Insert(entry);
    }

    for (uint32_t i = 0; i < kNumElements; i++) {
        // Ensure evicted entry is expected.
        ASSERT(lruCache.Evict() == insertedEntries[i]);
    }
}

TEST_F(LRUTests, InsertAndEvictWithDeletions) {
    LRUCache lruCache;
    std::vector<LRUEntry*> insertedEntries;

    for (uint32_t i = 0; i < kNumElements; i++) {
        // Create an LRUEntry.
        LRUEntry* entry = new LRUEntry(nullptr, 1);
        // Save the LRUEntry's address.
        insertedEntries.push_back(entry);
        // Insert LRUEntry into LRUCache.
        lruCache.Insert(entry);
    }

    // Remove every other entry from the LRU.
    for (uint32_t i = 0; i < kNumElements; i += 2) {
        lruCache.Remove(insertedEntries[i]);
    }

    for (uint32_t i = 0; i < kNumElements / 2; i++) {
        ASSERT(lruCache.Evict() == insertedEntries[(i * 2) + 1]);
    }
}

TEST_F(LRUTests, OverEvict) {
    LRUCache lruCache;
    std::vector<LRUEntry*> insertedEntries;

    // Create an LRUEntry
    LRUEntry* entry = new LRUEntry(nullptr, 1);
    // Save the LRUEntry's address
    insertedEntries.push_back(entry);
    // Insert LRUEntry into LRUCache
    lruCache.Insert(entry);

    // Evict once, leaving an empty cache
    lruCache.Evict();

    // Evict once more, which should return nullptr
    ASSERT(lruCache.Evict() == nullptr);
}
