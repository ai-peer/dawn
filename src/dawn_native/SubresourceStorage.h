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

#ifndef DAWNNATIVE_SUBRESOURCESTORAGE_H_
#define DAWNNATIVE_SUBRESOURCESTORAGE_H_

#include "common/Assert.h"
#include "dawn_native/EnumMaskIterator.h"
#include "dawn_native/Subresource.h"

#include <array>
#include <limits>
#include <memory>
#include <vector>

#include "common/Log.h"

namespace dawn_native {

    // SubresourceStorage<T> acts like a simple map from subresource (aspect, layer, level) to a
    // value of type T except that it tries to compress similar subresources so that algorithms
    // can act on a whole range of subresources at once if they have the same state.
    //
    // For example a very common case to optimize for is the tracking of the usage of texture
    // subresources inside a render pass: the vast majority of texture views will select the whole
    // texture while a small minority will select a sub-range. We want to optimize the common case
    // by setting and checking a single "usage" value when a full subresource is used but at the
    // same time allow per-subresource data when needed.
    //
    // Another example is barrier tracking per-subresource in the backends: it will often happen
    // that during texture upload each mip level will have a different "barrier state". However
    // when the texture is fully uploaded and after it is used for sampling (with a full view) for
    // the first time, the barrier state will likely be the same across all the subresources.
    // That's why some form of "recompression" of subresource state must be possibe.
    //
    // In order to keep the implementation details private and to avoid iterator-hell, this
    // container uses a more functional approach of calling a closure on the interesting ranges.
    // This is for example how to look at the state of all subresources.
    //
    //   subresources.Iterate([](const SubresourceRange& range, const T& data) {
    //      // Do something with the knowledge that all the subresources in `range` have value
    //      // `data`.
    //   });
    //
    // SubresourceStorage internally tracks compression state per aspect and then per layer of each
    // aspect. This means that a 2-aspect texture can have the following compression state:
    //
    //  - Aspect 0 is fully compressed.
    //  - Aspect 1 is partially compressed:
    //    - Aspect 1 layer 3 is decompressed.
    //    - Aspect 1 layer 0-2 and 4-42 are compressed.
    //
    // The choice of having secondary compression for array layers is to optimize for the cases
    // where transfer operations are used to update specific layers of texture with render or
    // transfer operations, while the rest is untouched. It seems much less likely that there
    // would be operations that touch all Nth mips of a 2D array texture without touching the
    // others.
    //
    // T must be a copyable type that supports equality comparison with ==.
    //
    // TODO(cwallez@chromium.org): Add the Merge() operation.
    // TODO(cwallez@chromium.org): Inline the storage for aspects to avoid allocating when
    // possible.
    template <typename T>
    class SubresourceStorage {
      public:
        // Creates the storage with the given "dimensions" and all subresources starting with the
        // initial value.
        SubresourceStorage(Aspect aspects,
                           uint32_t arrayLayerCount,
                           uint32_t mipLevelCount,
                           T initialValue = {});

        // Returns the data for a single subresource. Note that the reference returned might be the
        // same for multiple subresources.
        const T& Get(Aspect aspect, uint32_t arrayLayer, uint32_t mipLevel) const;

        // Given an iterateFunc that's a function or function-like objet that call be called with
        // arguments of type (const SubresourceRange& range, const T& data) and returns void,
        // calls it with aggregate ranges if possible, such that each subresource is part of
        // exactly one of the ranges iterateFunc is called with (and obviously data is the value
        // stored for that subresource). For example:
        //
        //   subresources.Iterate([&](const SubresourceRange& range, const T& data) {
        //       // Do something with range and data.
        //   });
        template <typename F>
        void Iterate(F&& iterateFunc) const;

        // Given an updateFunc that's a function or function-like objet that call be called with
        // arguments of type (const SubresourceRange& range, T* data) and returns void,
        // calls it with ranges that in aggregate form `range` and pass for each of the
        // sub-ranges a pointer to modify the value for that sub-range. For example:
        //
        //   subresources.Update(view->GetRange(), [](const SubresourceRange&, T* data) {
        //       *data |= wgpu::TextureUsage::Stuff;
        //   });
        template <typename F>
        void Update(const SubresourceRange& range, F&& updateFunc);

        // Other operations to consider:
        //
        //  - Merge(Range, SubresourceStorage<U>, mergeFunc) that takes the values from the other
        //    storage and modifies the value of the current storage with it.
        //  - UpdateTo(Range, T) that updates the range to a constant value.

        // Methods to query the internal state of SubresourceStorage for testing.
        Aspect GetAspectsForTesting() const;
        uint32_t GetArrayLayerCountForTesting() const;
        uint32_t GetMipLevelCountForTesting() const;
        bool IsAspectCompressedForTesting(Aspect aspect) const;
        bool IsLayerCompressedForTesting(Aspect aspect, uint32_t layer) const;

      private:
        void DecompressAspect(uint32_t aspectIndex);
        void RecompressAspect(uint32_t aspectIndex);

        void DecompressLayer(uint32_t aspectIndex, uint32_t layer);
        void RecompressLayer(uint32_t aspectIndex, uint32_t layer);

        SubresourceRange GetFullLayerRange(Aspect aspect, uint32_t layer) const;

        Aspect mAspects;
        uint8_t mMipLevelCount;
        uint16_t mArrayLayerCount;

        // Invariant: if an aspect is marked compressed, then all it's layers are marked as
        // compressed.
        static constexpr size_t kMaxAspects = 2;
        std::array<bool, kMaxAspects> mAspectCompressed;
        // Indexed as mLayerCompressed[aspectIndex * mArrayLayerCount + layer].
        std::unique_ptr<bool[]> mLayerCompressed;

        // Indexed as mData[(aspectIndex * mArrayLayerCount + layer) * mMipLevelCount + level].
        std::unique_ptr<T[]> mData;
    };

    template <typename T>
    SubresourceStorage<T>::SubresourceStorage(Aspect aspects,
                                              uint32_t arrayLayerCount,
                                              uint32_t mipLevelCount,
                                              T initialValue)
        : mAspects(aspects), mMipLevelCount(mipLevelCount), mArrayLayerCount(arrayLayerCount) {
        ASSERT(arrayLayerCount <= std::numeric_limits<decltype(mArrayLayerCount)>::max());
        ASSERT(mipLevelCount <= std::numeric_limits<decltype(mMipLevelCount)>::max());

        uint32_t aspectCount = GetAspectCount(aspects);
        ASSERT(aspectCount <= kMaxAspects);

        mLayerCompressed = std::make_unique<bool[]>(aspectCount * mArrayLayerCount);
        mData = std::make_unique<T[]>(aspectCount * mArrayLayerCount * mMipLevelCount);

        for (uint32_t aspectIndex = 0; aspectIndex < aspectCount; aspectIndex++) {
            mAspectCompressed[aspectIndex] = true;
            mData[aspectIndex * mArrayLayerCount * mMipLevelCount] = initialValue;
        }

        for (uint32_t layerIndex = 0; layerIndex < aspectCount * mArrayLayerCount; layerIndex++) {
            mLayerCompressed[layerIndex] = true;
        }
    }

    template <typename T>
    template <typename F>
    void SubresourceStorage<T>::Update(const SubresourceRange& range, F&& updateFunc) {
        bool fullLayers = range.baseMipLevel == 0 && range.levelCount == mMipLevelCount;
        bool fullAspects =
            range.baseArrayLayer == 0 && range.layerCount == mArrayLayerCount && fullLayers;

        for (Aspect aspect : IterateEnumMask(range.aspects)) {
            uint32_t aspectIndex = GetAspectIndex(aspect);
            size_t aspectDataIndex = aspectIndex * mArrayLayerCount * mMipLevelCount;

            // Call the updateFunc once for the whole aspect if possible or decompress and fallback
            // to per-layer handling.
            if (mAspectCompressed[aspectIndex]) {
                if (fullAspects) {
                    SubresourceRange updateRange =
                        SubresourceRange::MakeFull(aspect, mArrayLayerCount, mMipLevelCount);
                    updateFunc(updateRange, &mData[aspectDataIndex]);
                    continue;
                }
                DecompressAspect(aspectIndex);
            }

            uint32_t layerEnd = range.baseArrayLayer + range.layerCount;
            for (uint32_t layer = range.baseArrayLayer; layer < layerEnd; layer++) {
                size_t layerDataIndex = aspectDataIndex + layer * mMipLevelCount;

                // Call the updateFunc once for the whole layer if possible or decompress and
                // fallback to per-level handling.
                if (mLayerCompressed[aspectIndex * mArrayLayerCount + layer]) {
                    if (fullLayers) {
                        SubresourceRange updateRange = GetFullLayerRange(aspect, layer);
                        updateFunc(updateRange, &mData[layerDataIndex]);
                        continue;
                    }
                    DecompressLayer(aspectIndex, layer);
                }

                // Worst case: call updateFunc per level.
                uint32_t levelEnd = range.baseMipLevel + range.levelCount;
                for (uint32_t level = range.baseMipLevel; level < levelEnd; level++) {
                    SubresourceRange updateRange =
                        SubresourceRange::MakeSingle(aspect, layer, level);
                    updateFunc(updateRange, &mData[layerDataIndex + level]);
                }

                // If the range has fullLayers then it is likely we can recompress after the calls
                // to updateFunc (this branch is skipped if updateFunc was called for the whole
                // layer).
                if (fullLayers) {
                    RecompressLayer(aspectIndex, layer);
                }
            }

            // If the range has fullAspects then it is likely we can recompress after the calls to
            // updateFunc (this branch is skipped if updateFunc was called for the whole aspect).
            if (fullAspects) {
                RecompressAspect(aspectIndex);
            }
        }
    }

    template <typename T>
    template <typename F>
    void SubresourceStorage<T>::Iterate(F&& iterateFunc) const {
        for (Aspect aspect : IterateEnumMask(mAspects)) {
            uint32_t aspectIndex = GetAspectIndex(aspect);
            size_t aspectDataIndex = aspectIndex * mArrayLayerCount * mMipLevelCount;

            // Fastest path, call iterateFunc on the whole aspect at once.
            if (mAspectCompressed[aspectIndex]) {
                SubresourceRange range =
                    SubresourceRange::MakeFull(aspect, mArrayLayerCount, mMipLevelCount);
                iterateFunc(range, mData[aspectDataIndex]);
                continue;
            }

            for (uint32_t layer = 0; layer < mArrayLayerCount; layer++) {
                size_t layerDataIndex = aspectDataIndex + layer * mMipLevelCount;

                // Fast path, call iterateFunc on the whole array layer at once.
                if (mLayerCompressed[aspectIndex * mArrayLayerCount + layer]) {
                    SubresourceRange range = GetFullLayerRange(aspect, layer);
                    iterateFunc(range, mData[layerDataIndex]);
                    continue;
                }

                // Slow path, call iterateFunc for each mip level.
                for (uint32_t level = 0; level < mMipLevelCount; level++) {
                    SubresourceRange range = SubresourceRange::MakeSingle(aspect, layer, level);
                    iterateFunc(range, mData[layerDataIndex + level]);
                }
            }
        }
    }

    template <typename T>
    const T& SubresourceStorage<T>::Get(Aspect aspect,
                                        uint32_t arrayLayer,
                                        uint32_t mipLevel) const {
        uint32_t aspectIndex = GetAspectIndex(aspect);
        ASSERT(aspectIndex < GetAspectCount(mAspects));
        ASSERT(arrayLayer < mArrayLayerCount);
        ASSERT(mipLevel < mMipLevelCount);

        // Fastest path, the aspect is compressed!
        uint32_t dataIndex = aspectIndex * mArrayLayerCount * mMipLevelCount;
        if (mAspectCompressed[aspectIndex]) {
            return mData[dataIndex];
        }

        // Fast path, the array layer is compressed.
        dataIndex += arrayLayer * mMipLevelCount;
        if (mLayerCompressed[aspectIndex * mArrayLayerCount + arrayLayer]) {
            return mData[dataIndex];
        }

        return mData[dataIndex + mipLevel];
    }

    template <typename T>
    Aspect SubresourceStorage<T>::GetAspectsForTesting() const {
        return mAspects;
    }

    template <typename T>
    uint32_t SubresourceStorage<T>::GetArrayLayerCountForTesting() const {
        return mArrayLayerCount;
    }

    template <typename T>
    uint32_t SubresourceStorage<T>::GetMipLevelCountForTesting() const {
        return mMipLevelCount;
    }

    template <typename T>
    bool SubresourceStorage<T>::IsAspectCompressedForTesting(Aspect aspect) const {
        return mAspectCompressed[GetAspectIndex(aspect)];
    }

    template <typename T>
    bool SubresourceStorage<T>::IsLayerCompressedForTesting(Aspect aspect, uint32_t layer) const {
        return mLayerCompressed[GetAspectIndex(aspect) * mArrayLayerCount + layer];
    }

    template <typename T>
    void SubresourceStorage<T>::DecompressAspect(uint32_t aspectIndex) {
        ASSERT(mAspectCompressed[aspectIndex]);

        ASSERT(mLayerCompressed[aspectIndex * mArrayLayerCount + 0]);
        size_t aspectDataIndex = aspectIndex * mArrayLayerCount * mMipLevelCount;
        for (uint32_t layer = 1; layer < mArrayLayerCount; layer++) {
            mData[aspectDataIndex + layer * mMipLevelCount] = mData[aspectDataIndex];
            ASSERT(mLayerCompressed[aspectIndex * mArrayLayerCount + layer]);
        }

        mAspectCompressed[aspectIndex] = false;
    }

    template <typename T>
    void SubresourceStorage<T>::RecompressAspect(uint32_t aspectIndex) {
        ASSERT(!mAspectCompressed[aspectIndex]);

        size_t aspectDataIndex = aspectIndex * mArrayLayerCount * mMipLevelCount;
        for (uint32_t layer = 1; layer < mArrayLayerCount; layer++) {
            if (mData[aspectDataIndex + layer * mMipLevelCount] != mData[aspectDataIndex]) {
                return;
            }
        }

        mAspectCompressed[aspectIndex] = true;
    }

    template <typename T>
    void SubresourceStorage<T>::DecompressLayer(uint32_t aspectIndex, uint32_t layer) {
        ASSERT(mLayerCompressed[aspectIndex * mArrayLayerCount + layer]);
        ASSERT(!mAspectCompressed[aspectIndex]);

        size_t layerDataIndex = (aspectIndex * mArrayLayerCount + layer) * mMipLevelCount;
        for (uint32_t level = 1; level < mMipLevelCount; level++) {
            mData[layerDataIndex + level] = mData[layerDataIndex];
        }

        mLayerCompressed[aspectIndex * mArrayLayerCount + layer] = false;
    }

    template <typename T>
    void SubresourceStorage<T>::RecompressLayer(uint32_t aspectIndex, uint32_t layer) {
        ASSERT(!mLayerCompressed[aspectIndex * mArrayLayerCount + layer]);
        ASSERT(!mAspectCompressed[aspectIndex]);

        size_t layerDataIndex = (aspectIndex * mArrayLayerCount + layer) * mMipLevelCount;
        for (uint32_t level = 1; level < mMipLevelCount; level++) {
            if (mData[layerDataIndex + level] != mData[layerDataIndex]) {
                return;
            }
        }

        mLayerCompressed[aspectIndex * mArrayLayerCount + layer] = true;
    }

    template <typename T>
    SubresourceRange SubresourceStorage<T>::GetFullLayerRange(Aspect aspect, uint32_t layer) const {
        return {aspect, {layer, 1}, {0, mMipLevelCount}};
    }

}  // namespace dawn_native

#endif  // DAWNNATIVE_SUBRESOURCESTORAGE_H_
