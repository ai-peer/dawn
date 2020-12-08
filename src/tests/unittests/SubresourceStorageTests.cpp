// Copyright 2020 The Dawn Authors
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

#include "dawn_native/SubresourceStorage.h"

#include "common/Log.h"

using namespace dawn_native;

// A fake class that replicates the behavior of SubresourceStorage but without any compression and
// is used to compare the results of operations on SubresourceStorage against the "ground truth" of
// FakeStorage.
template <typename T>
struct FakeStorage {
    FakeStorage(Aspect aspects,
                uint32_t arrayLayerCount,
                uint32_t mipLevelCount,
                T initialValue = {})
        : mAspects(aspects),
          mArrayLayerCount(arrayLayerCount),
          mMipLevelCount(mipLevelCount),
          mData(GetAspectCount(aspects) * arrayLayerCount * mipLevelCount, initialValue) {
    }

    template <typename F>
    void Update(const SubresourceRange& range, F&& updateFunc) {
        for (Aspect aspect : IterateEnumMask(range.aspects)) {
            for (uint32_t layer = range.baseArrayLayer;
                 layer < range.baseArrayLayer + range.layerCount; layer++) {
                for (uint32_t level = range.baseMipLevel;
                     level < range.baseMipLevel + range.levelCount; level++) {
                    SubresourceRange range = SubresourceRange::MakeSingle(aspect, layer, level);
                    updateFunc(range, &mData[GetDataIndex(aspect, layer, level)]);
                }
            }
        }
    }

    const T& Get(Aspect aspect, uint32_t arrayLayer, uint32_t mipLevel) const {
        return mData[GetDataIndex(aspect, arrayLayer, mipLevel)];
    }

    size_t GetDataIndex(Aspect aspect, uint32_t layer, uint32_t level) const {
        uint32_t aspectIndex = GetAspectIndex(aspect);
        return level + mMipLevelCount * (layer + mArrayLayerCount * aspectIndex);
    }

    // Method that checks that this and real have exactly the same content. It does so via looping
    // on all subresources and calling Get() (hence testing Get()). It also calls Iterate()
    // checking that every subresource is mentioned exactly once and that its content is correct
    // (hence testing Iterate()).
    // Its implementation requires the RangeTracker below that itself needs FakeStorage<int> so it
    // cannot be define inline with the other methods.
    void CheckSameAs(const SubresourceStorage<T>& real);

    Aspect mAspects;
    uint32_t mArrayLayerCount;
    uint32_t mMipLevelCount;

    std::vector<T> mData;
};

// Track a set of ranges that have been seen and can assert that in aggregate they make exactly
// a single range (and that each subresource was seen only once).
struct RangeTracker {
    template <typename T>
    RangeTracker(const SubresourceStorage<T>& s)
        : mTracked(s.GetAspectsForTesting(),
                   s.GetArrayLayerCountForTesting(),
                   s.GetMipLevelCountForTesting(),
                   0) {
    }

    void Track(const SubresourceRange& range) {
        // Add +1 to the subresources tracked.
        mTracked.Update(range, [](const SubresourceRange&, uint32_t* counter) {
            ASSERT_EQ(*counter, 0u);
            *counter += 1;
        });
    }

    void CheckTrackedExactly(const SubresourceRange& range) {
        // Check that all subresources in the range were tracked once and set the counter back to 0.
        mTracked.Update(range, [](const SubresourceRange&, uint32_t* counter) {
            ASSERT_EQ(*counter, 1u);
            *counter = 0;
        });

        // Now all subresources should be at 0.
        for (int counter : mTracked.mData) {
            ASSERT_EQ(counter, 0);
        }
    }

    FakeStorage<uint32_t> mTracked;
};

template <typename T>
void FakeStorage<T>::CheckSameAs(const SubresourceStorage<T>& real) {
    EXPECT_EQ(real.GetAspectsForTesting(), mAspects);
    EXPECT_EQ(real.GetArrayLayerCountForTesting(), mArrayLayerCount);
    EXPECT_EQ(real.GetMipLevelCountForTesting(), mMipLevelCount);

    for (Aspect aspect : IterateEnumMask(mAspects)) {
        for (uint32_t layer = 0; layer < mArrayLayerCount; layer++) {
            for (uint32_t level = 0; level < mMipLevelCount; level++) {
                ASSERT_EQ(real.Get(aspect, layer, level), Get(aspect, layer, level));
            }
        }
    }

    RangeTracker tracker(real);
    real.Iterate([this, &tracker](const SubresourceRange& range, const T& data) {
        // Check that the range is sensical.
        EXPECT_TRUE(IsSubset(range.aspects, mAspects));

        EXPECT_LT(range.baseArrayLayer, mArrayLayerCount);
        EXPECT_LE(range.baseArrayLayer + range.layerCount, mArrayLayerCount);

        EXPECT_LT(range.baseMipLevel, mMipLevelCount);
        EXPECT_LE(range.baseMipLevel + range.levelCount, mMipLevelCount);

        tracker.Track(range);
    });

    tracker.CheckTrackedExactly(
        SubresourceRange::MakeFull(mAspects, mArrayLayerCount, mMipLevelCount));
}

template <typename T>
void CheckAspectCompressed(const SubresourceStorage<T>& s, Aspect aspect, bool expected) {
    ASSERT(HasOneBit(aspect));

    uint32_t levelCount = s.GetMipLevelCountForTesting();
    uint32_t layerCount = s.GetArrayLayerCountForTesting();

    bool seen = false;
    s.Iterate([&](const SubresourceRange& range, const T&) {
        if (range.aspects == aspect && range.layerCount == layerCount &&
            range.levelCount == levelCount && range.baseArrayLayer == 0 &&
            range.baseMipLevel == 0) {
            seen = true;
        }
    });

    ASSERT_EQ(seen, expected);

    // Check that the internal state of SubresourceStorage matches what we expect.
    // If an aspect is compressed, all its layers should be internally tagged as compressed.
    ASSERT_EQ(s.IsAspectCompressedForTesting(aspect), expected);
    if (expected) {
        for (uint32_t layer = 0; layer < s.GetArrayLayerCountForTesting(); layer++) {
            ASSERT_TRUE(s.IsLayerCompressedForTesting(aspect, layer));
        }
    }
}

template <typename T>
void CheckLayerCompressed(const SubresourceStorage<T>& s,
                          Aspect aspect,
                          uint32_t layer,
                          bool expected) {
    ASSERT(HasOneBit(aspect));

    uint32_t levelCount = s.GetMipLevelCountForTesting();

    bool seen = false;
    s.Iterate([&](const SubresourceRange& range, const T&) {
        if (range.aspects == aspect && range.layerCount == 1 && range.levelCount == levelCount &&
            range.baseArrayLayer == layer && range.baseMipLevel == 0) {
            seen = true;
        }
    });

    ASSERT_EQ(seen, expected);
    ASSERT_EQ(s.IsLayerCompressedForTesting(aspect, layer), expected);
}

struct SmallData {
    uint32_t value = 0xF00;
};

bool operator==(const SmallData& a, const SmallData& b) {
    return a.value == b.value;
}

// Test that the default value is correctly set.
TEST(SubresourceStorageTest, DefaultValue) {
    // Test setting no default value for a primitive type.
    {
        SubresourceStorage<int> s(Aspect::Color, 3, 5);
        EXPECT_EQ(s.Get(Aspect::Color, 1, 2), 0);

        FakeStorage<int> f(Aspect::Color, 3, 5);
        f.CheckSameAs(s);
    }

    // Test setting a default value for a primitive type.
    {
        SubresourceStorage<int> s(Aspect::Color, 3, 5, 42);
        EXPECT_EQ(s.Get(Aspect::Color, 1, 2), 42);

        FakeStorage<int> f(Aspect::Color, 3, 5, 42);
        f.CheckSameAs(s);
    }

    // Test setting no default value for a type with a default constructor.
    {
        SubresourceStorage<SmallData> s(Aspect::Color, 3, 5);
        EXPECT_EQ(s.Get(Aspect::Color, 1, 2).value, 0xF00u);

        FakeStorage<SmallData> f(Aspect::Color, 3, 5);
        f.CheckSameAs(s);
    }
    // Test setting a default value for a type with a default constructor.
    {
        SubresourceStorage<SmallData> s(Aspect::Color, 3, 5, {007u});
        EXPECT_EQ(s.Get(Aspect::Color, 1, 2).value, 007u);

        FakeStorage<SmallData> f(Aspect::Color, 3, 5, {007u});
        f.CheckSameAs(s);
    }
}

// The tests for Update() all follow the same pattern of setting up a real and a fake storage then
// performing one or multiple Update()s on them and checking:
//  - They have the same content.
//  - The Update() range was correct.
//  - The aspects and layers have the expected "compressed" status.

// Calls Update both on the read storage and the fake storage but intercepts the call to updateFunc
// done by the real storage to check their ranges argument aggregate to exactly the update range.
template <typename T, typename F>
void CallUpdateOnBoth(SubresourceStorage<T>* s,
                      FakeStorage<T>* f,
                      const SubresourceRange& range,
                      F&& updateFunc) {
    RangeTracker tracker(*s);

    s->Update(range, [&](const SubresourceRange& range, T* data) {
        tracker.Track(range);
        updateFunc(range, data);
    });
    f->Update(range, updateFunc);

    tracker.CheckTrackedExactly(range);
}

// Test updating a single subresource on a single-aspect storage.
TEST(SubresourceStorageTest, SingleSubresourceUpdateSingleAspect) {
    SubresourceStorage<int> s(Aspect::Color, 5, 7);
    FakeStorage<int> f(Aspect::Color, 5, 7);

    // Update a single subresource.
    SubresourceRange range = SubresourceRange::MakeSingle(Aspect::Color, 3, 2);
    CallUpdateOnBoth(&s, &f, range, [](const SubresourceRange&, int* data) { *data += 1; });

    f.CheckSameAs(s);
    CheckAspectCompressed(s, Aspect::Color, false);
    CheckLayerCompressed(s, Aspect::Color, 2, true);
    CheckLayerCompressed(s, Aspect::Color, 3, false);
    CheckLayerCompressed(s, Aspect::Color, 4, true);
}

// Test updating a single subresource on a multi-aspect storage.
TEST(SubresourceStorageTest, SingleSubresourceUpdateMultiAspect) {
    SubresourceStorage<int> s(Aspect::Depth | Aspect::Stencil, 5, 3);
    FakeStorage<int> f(Aspect::Depth | Aspect::Stencil, 5, 3);

    SubresourceRange range = SubresourceRange::MakeSingle(Aspect::Stencil, 1, 2);
    CallUpdateOnBoth(&s, &f, range, [](const SubresourceRange&, int* data) { *data += 1; });

    f.CheckSameAs(s);
    CheckAspectCompressed(s, Aspect::Depth, true);
    CheckAspectCompressed(s, Aspect::Stencil, false);
    CheckLayerCompressed(s, Aspect::Stencil, 0, true);
    CheckLayerCompressed(s, Aspect::Stencil, 1, false);
    CheckLayerCompressed(s, Aspect::Stencil, 2, true);
}

// Test updating as a stipple pattern on one of two aspects then updating it completely.
TEST(SubresourceStorageTest, UpdateStipple) {
    const uint32_t kLayers = 10;
    const uint32_t kLevels = 7;
    SubresourceStorage<int> s(Aspect::Depth | Aspect::Stencil, kLayers, kLevels);
    FakeStorage<int> f(Aspect::Depth | Aspect::Stencil, kLayers, kLevels);

    // Update with a stipple.
    for (uint32_t layer = 0; layer < kLayers; layer++) {
        for (uint32_t level = 0; level < kLevels; level++) {
            if ((layer + level) % 2 == 0) {
                SubresourceRange range = SubresourceRange::MakeSingle(Aspect::Depth, layer, level);
                CallUpdateOnBoth(&s, &f, range,
                                 [](const SubresourceRange&, int* data) { *data += 17; });
            }
        }
    }

    // The depth should be fully uncompressed while the stencil stayed compressed.
    f.CheckSameAs(s);
    CheckAspectCompressed(s, Aspect::Stencil, true);
    CheckAspectCompressed(s, Aspect::Depth, false);
    for (uint32_t layer = 0; layer < kLayers; layer++) {
        CheckLayerCompressed(s, Aspect::Depth, layer, false);
    }

    // Update completely with a single value. Recompression should happen!
    {
        SubresourceRange fullRange =
            SubresourceRange::MakeFull(Aspect::Depth | Aspect::Stencil, kLayers, kLevels);
        CallUpdateOnBoth(&s, &f, fullRange, [](const SubresourceRange&, int* data) { *data = 31; });
    }

    f.CheckSameAs(s);
    CheckAspectCompressed(s, Aspect::Depth, true);
    CheckAspectCompressed(s, Aspect::Stencil, true);
}

// Test updating as a crossing band pattern:
//  - The first band is full layers [2, 3] on both aspects
//  - The second band is full mips [5, 6] on one aspect.
// Then updating completely.
TEST(SubresourceStorageTest, UpdateTwoBand) {
    const uint32_t kLayers = 5;
    const uint32_t kLevels = 9;
    SubresourceStorage<int> s(Aspect::Depth | Aspect::Stencil, kLayers, kLevels);
    FakeStorage<int> f(Aspect::Depth | Aspect::Stencil, kLayers, kLevels);

    // Update the two bands
    {
        SubresourceRange range(Aspect::Depth | Aspect::Stencil, {2, 2}, {0, kLevels});
        CallUpdateOnBoth(&s, &f, range, [](const SubresourceRange&, int* data) { *data += 3; });
    }

    // The layers were fully updated so they should stay compressed.
    f.CheckSameAs(s);
    CheckLayerCompressed(s, Aspect::Depth, 2, true);
    CheckLayerCompressed(s, Aspect::Depth, 3, true);
    CheckLayerCompressed(s, Aspect::Stencil, 2, true);
    CheckLayerCompressed(s, Aspect::Stencil, 3, true);

    {
        SubresourceRange range(Aspect::Depth | Aspect::Stencil, {0, kLayers}, {5, 2});
        CallUpdateOnBoth(&s, &f, range, [](const SubresourceRange&, int* data) { *data *= 3; });
    }

    // The layers had to be decompressed.
    f.CheckSameAs(s);
    CheckLayerCompressed(s, Aspect::Depth, 2, false);
    CheckLayerCompressed(s, Aspect::Depth, 3, false);
    CheckLayerCompressed(s, Aspect::Stencil, 2, false);
    CheckLayerCompressed(s, Aspect::Stencil, 3, false);

    // Update completely. Without a single value recompression shouldn't happen.
    {
        SubresourceRange fullRange =
            SubresourceRange::MakeFull(Aspect::Depth | Aspect::Stencil, kLayers, kLevels);
        CallUpdateOnBoth(&s, &f, fullRange,
                         [](const SubresourceRange&, int* data) { *data += 12; });
    }

    f.CheckSameAs(s);
    CheckAspectCompressed(s, Aspect::Depth, false);
    CheckAspectCompressed(s, Aspect::Stencil, false);
}

// Test updating with extremal subresources
//    - Then half of the array layers in full.
//    - Then updating completely.
TEST(SubresourceStorageTest, UpdateExtremas) {
    const uint32_t kLayers = 6;
    const uint32_t kLevels = 4;
    SubresourceStorage<int> s(Aspect::Color, kLayers, kLevels);
    FakeStorage<int> f(Aspect::Color, kLayers, kLevels);

    // Update the two extrema
    {
        SubresourceRange range = SubresourceRange::MakeSingle(Aspect::Color, 0, kLevels - 1);
        CallUpdateOnBoth(&s, &f, range, [](const SubresourceRange&, int* data) { *data += 3; });
    }
    {
        SubresourceRange range = SubresourceRange::MakeSingle(Aspect::Color, kLayers - 1, 0);
        CallUpdateOnBoth(&s, &f, range, [](const SubresourceRange&, int* data) { *data *= 3; });
    }

    f.CheckSameAs(s);
    CheckLayerCompressed(s, Aspect::Color, 0, false);
    CheckLayerCompressed(s, Aspect::Color, 1, true);
    CheckLayerCompressed(s, Aspect::Color, kLayers - 2, true);
    CheckLayerCompressed(s, Aspect::Color, kLayers - 1, false);

    // Update half of the layers in full with constant values. Some recompression should happen.
    {
        SubresourceRange range(Aspect::Color, {0, kLayers / 2}, {0, kLevels});
        CallUpdateOnBoth(&s, &f, range, [](const SubresourceRange&, int* data) { *data = 123; });
    }

    f.CheckSameAs(s);
    CheckLayerCompressed(s, Aspect::Color, 0, true);
    CheckLayerCompressed(s, Aspect::Color, 1, true);
    CheckLayerCompressed(s, Aspect::Color, kLayers - 1, false);

    // Update completely. Recompression should happen!
    {
        SubresourceRange fullRange = SubresourceRange::MakeFull(Aspect::Color, kLayers, kLevels);
        CallUpdateOnBoth(&s, &f, fullRange, [](const SubresourceRange&, int* data) { *data = 35; });
    }

    f.CheckSameAs(s);
    CheckAspectCompressed(s, Aspect::Color, true);
}

// Bugs found while testing:
//  - mLayersCompressed not initialized to true.
//  - DecompressLayer setting Compressed to true instead of false.
//  - Get() checking for !compressed instead of compressed for the early exit.
//  - ASSERT in RecompressLayers was inverted.
