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

#include "dawn_native/Texture.h"

namespace {

    using UninitializedSubresources = dawn_native::TextureBase::UninitializedSubresources;
    using SubresourceRange = dawn_native::TextureBase::SubresourceRange;

    class UninitializedSubresourceIteratorTest : public ::testing::Test {
      protected:
        class FakeTexture {
          public:
            FakeTexture(uint32_t mipLevelCount, uint32_t arrayLayerCount, bool isInitialized)
                : mMipLevelCount(mipLevelCount),
                  mInitializedState(mipLevelCount * arrayLayerCount, isInitialized) {
            }

            UninitializedSubresources IterateUninitializedSubresources(
                SubresourceRange range) const {
                return UninitializedSubresources(mInitializedState, range, mMipLevelCount);
            }

            void SetInitialized(SubresourceRange range, bool isInitialized) {
                for (uint32_t mipLevel = range.baseMipLevel;
                     mipLevel < range.baseMipLevel + range.mipLevelCount; ++mipLevel) {
                    for (uint32_t arrayLayer = range.baseArrayLayer;
                         arrayLayer < range.baseArrayLayer + range.arrayLayerCount; ++arrayLayer) {
                        mInitializedState[arrayLayer * mMipLevelCount + mipLevel] = isInitialized;
                    }
                }
            }

          private:
            const uint32_t mMipLevelCount;
            std::vector<bool> mInitializedState;
        };

        void ExpectIteratedRanges(const FakeTexture& texture,
                                  SubresourceRange baseRange,
                                  std::vector<SubresourceRange> expectedRanges) {
            uint32_t rangeCount = 0;
            for (SubresourceRange range : texture.IterateUninitializedSubresources(baseRange)) {
                EXPECT_EQ(range, expectedRanges[rangeCount++]);
            }
            EXPECT_EQ(expectedRanges.size(), rangeCount);
        }

        void ExpectSingleRange(const FakeTexture& texture, SubresourceRange expectedRange) {
            ExpectIteratedRanges(texture, expectedRange, {expectedRange});
        }
    };

}  // namespace

// Test iterating over an initialized texture with one subresource.
TEST_F(UninitializedSubresourceIteratorTest, SingleSubresourceInitialized) {
    FakeTexture texture(1, 1, true);
    ExpectIteratedRanges(texture, {0, 1, 0, 1}, {});
}

// Test iterating over an initialized texture with multiple subresource.
TEST_F(UninitializedSubresourceIteratorTest, MultipleSubresourceInitialized) {
    FakeTexture texture(6, 7, true);
    ExpectIteratedRanges(texture, {0, 6, 0, 7}, {});
}

// Test iterating over an initialized subresources of a largely uninitialized texture.
TEST_F(UninitializedSubresourceIteratorTest, SingleSubresourceInitializedOfUninitializedTexture) {
    FakeTexture texture(6, 7, false);
    texture.SetInitialized({2, 1, 3, 1}, true);

    ExpectIteratedRanges(texture, {2, 1, 3, 1}, {});
}

// Test iterating over multiple initialized subresources of a largely uninitialized texture.
TEST_F(UninitializedSubresourceIteratorTest, MultipleSubresourceInitializedOfUninitializedTexture) {
    FakeTexture texture(6, 7, false);
    texture.SetInitialized({1, 3, 0, 2}, true);

    // Test the initialized range.
    ExpectIteratedRanges(texture, {1, 3, 0, 2}, {});

    // Test a beginning subset of the initialized range.
    ExpectIteratedRanges(texture, {1, 1, 0, 1}, {});

    // Test an ending subset of the initialized range.
    ExpectIteratedRanges(texture, {2, 1, 1, 1}, {});
}

// Test iterating over an uninitialized texture with one subresource.
TEST_F(UninitializedSubresourceIteratorTest, SingleSubresourceUninitialized) {
    FakeTexture texture(1, 1, false);
    ExpectSingleRange(texture, {0, 1, 0, 1});
}

// Test iterating over an uninitialized texture with multiple subresource.
TEST_F(UninitializedSubresourceIteratorTest, MultipleSubresourceUninitialized) {
    FakeTexture texture(6, 7, false);
    ExpectSingleRange(texture, {0, 6, 0, 7});
}

// Test iterating over an uninitialized subresources of a largely initialized texture.
TEST_F(UninitializedSubresourceIteratorTest, SingleSubresourceUninitializedOfInitializedTexture) {
    FakeTexture texture(6, 7, true);
    texture.SetInitialized({2, 1, 3, 1}, false);

    // Iterating the uninitialized range yields exactly the range.
    ExpectSingleRange(texture, {2, 1, 3, 1});

    // Iterating the entire range yields only the uninitialized range.
    ExpectIteratedRanges(texture, {0, 6, 0, 7}, {{2, 1, 3, 1}});
}

// Test iterating over multiple uninitialized subresources of a largely initialized texture.
TEST_F(UninitializedSubresourceIteratorTest, MultipleSubresourceUninitializedOfInitializedTexture) {
    FakeTexture texture(6, 7, true);
    texture.SetInitialized({1, 3, 0, 7}, false);
    // . . . . . . .
    // x x x x x x x
    // x x x x x x x
    // x x x x x x x
    // . . . . . . .
    // . . . . . . .

    // Iterating the uninitialized range yields exactly the range.
    ExpectSingleRange(texture, {1, 3, 0, 7});

    // Iterating a beginning subset of the uninitialized range yields exactly the subset.
    ExpectSingleRange(texture, {1, 1, 0, 1});

    // Iterating a ending subset of the uninitialized range yields exactly the subset.
    ExpectSingleRange(texture, {2, 1, 6, 1});

    // Iterating the entire range yields the uninitialized range.
    ExpectIteratedRanges(texture, {0, 6, 0, 7}, {{1, 3, 0, 7}});
}

// Test iterating over multiple uninitialized subresources of a largely initialized texture.
// The uninitialized region does not touch the edges of the base range and will be broken into
// pieces.
TEST_F(UninitializedSubresourceIteratorTest, MultipleSubresourceUninitializedNotTouchingEdges) {
    FakeTexture texture(6, 7, true);
    texture.SetInitialized({1, 3, 1, 4}, false);
    // . . . . . . .
    // . x x x x . .
    // . x x x x . .
    // . x x x x . .
    // . . . . . . .
    // . . . . . . .

    // Iterating the entire range yields the uninitialized ranges.
    ExpectIteratedRanges(texture, {0, 6, 0, 7}, {{1, 1, 1, 4}, {2, 1, 1, 4}, {3, 1, 1, 4}});
}

// Test iterating over a sparsely uninitialized texture.
TEST_F(UninitializedSubresourceIteratorTest, SparseUninitializedTexture) {
    FakeTexture texture(6, 7, true);
    texture.SetInitialized({0, 1, 0, 3}, false);
    texture.SetInitialized({0, 1, 4, 1}, false);
    texture.SetInitialized({2, 1, 1, 2}, false);
    texture.SetInitialized({2, 1, 4, 1}, false);
    texture.SetInitialized({4, 1, 0, 1}, false);
    texture.SetInitialized({4, 1, 2, 3}, false);
    texture.SetInitialized({5, 1, 3, 4}, false);
    // x x x . x x .
    // . . . . . . .
    // . x x . x . .
    // . . . . . . .
    // x . x x x . .
    // . . . x x x x

    // Iterating the entire range yields the uninitialized ranges.
    ExpectIteratedRanges(texture, {0, 6, 0, 7},
                         {
                             {0, 1, 0, 3},
                             {0, 1, 4, 1},
                             {2, 1, 1, 2},
                             {2, 1, 4, 1},
                             {4, 1, 0, 1},
                             {4, 1, 2, 3},
                             {5, 1, 3, 4},
                         });
}
