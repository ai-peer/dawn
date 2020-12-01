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
// TODO fix this.
#include "dawn_native/Texture.h"

#include <limits>
#include <vector>

namespace dawn_native {

    template <typename T>
    class SubresourceStorage {
      public:
        SubresourceStorage(Aspect aspects,
                           uint32_t arrayLayerCount,
                           uint32_t mipLevelCount,
                           T initialValue = {});

        template <typename F>
        void Update(const SubresourceRange& range, F&& updateFunc);

        // template <typename U, typename F>
        // void Merge(const SubresourceStorage<U>& other, F&& mergeFunc);

        template <typename F>
        void Iterate(F&& iterateFunc) const;

        const T& Get(Aspect aspect, uint32_t arrayLayer, uint32_t mipLevel) const;

        bool IsAspectCompressedForTesting(Aspect aspect) const;
        bool IsArrayLayerCompressedForTesting(Aspect aspect, uint32_t arrayLayer) const;

      private:
        void DecompressAspect(uint32_t aspectIndex);
        void RecompressAspect(uint32_t aspectIndex);

        void DecompressLayer(uint32_t aspectIndex, uint32_t layer);
        void RecompressLayers(uint32_t aspectIndex, uint32_t layer);

        SubresourceRange GetFullAspectRange(Aspect aspect) const;
        SubresourceRange GetFullLayerRange(Aspect aspect, uint32_t layer) const;

        // TODO move in subresources?
        static uint32_t GetAspectIndex(Aspect aspect);
        static uint32_t GetAspectCount(Aspect aspects);

        Aspect mAspects;
        uint8_t mMipLevelCount;
        uint16_t mArrayLayerCount;

        static constexpr size_t kMaxAspects = 2;
        std::array<bool, kMaxAspects> mAspectCompressed;

        std::unique_ptr<bool[]> mLayerCompressed;
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
    }

    template <typename T>
    template <typename F>
    void SubresourceStorage<T>::Update(const SubresourceRange& range, F&& updateFunc) {
        bool fullLayers = range.baseMipLevel == 0 && range.levelCount == mMipLevelCount;
        bool fullAspects =
            range.baseArrayLayer == 0 && range.layerCount == mArrayLayerCount && fullLayers;

        for (Aspect aspect : IterateEnumMask(range.aspects)) {
            uint32_t aspectIndex = GetAspectIndex(aspect);
            size_t aspectDataOffset = aspectIndex * mArrayLayerCount * mMipLevelCount;

            // Call the updateFunc once for the whole aspect if possible or decompress and fallback
            // to per-layer handling.
            if (mAspectCompressed[aspectIndex]) {
                if (fullAspects) {
                    SubresourceRange updateRange = GetFullAspectRange(aspect);
                    updateFunc(updateRange, &mData[aspectDataOffset]);
                    continue;
                }
                DecompressAspect(aspectIndex);
            }

            uint32_t layerEnd = range.baseArrayLayer + range.layerCount;
            for (uint32_t layer = range.baseArrayLayer; layer < layerEnd; layer++) {
                size_t layerDataOffset = aspectDataOffset + layer * mMipLevelCount;

                // Call the updateFunc once for the whole layer if possible or decompress and
                // fallback to per-level handling.
                if (mLayerCompressed[aspectIndex * mArrayLayerCount + layer]) {
                    if (fullLayers) {
                        SubresourceRange updateRange = GetFullLayerRange(aspect, layer);
                        updateFunc(updateRange, &mData[layerDataOffset]);
                        continue;
                    }
                    DecompressLayer(aspectIndex, layer);
                }

                // Worst case: call updateFunc per level.
                uint32_t levelEnd = range.baseMipLevel + range.levelCount;
                for (uint32_t level = range.baseMipLevel; level < levelEnd; level++) {
                    SubresourceRange updateRange =
                        SubresourceRange::SingleMipAndLayer(level, layer, aspect);
                    updateFunc(updateRange, &mData[layerDataOffset + level]);
                }

                // If the range has fullLayers then it is likely we can recompress after the calls
                // to updateFunc (this branch is skipped if updateFunc was called for the whole
                // layer).
                if (fullLayers) {
                    RecompressLayers(aspectIndex, layer);
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
            size_t aspectDataOffset = aspectIndex * mArrayLayerCount * mMipLevelCount;

            if (mAspectCompressed[aspectIndex]) {
                SubresourceRange range = GetFullAspectRange(aspect);
                iterateFunc(range, &mData[aspectDataOffset]);
                continue;
            }

            for (uint32_t layer = 0; layer < mArrayLayerCount; layer++) {
                size_t layerDataOffset = aspectDataOffset + layer * mMipLevelCount;

                if (mLayerCompressed[aspectIndex * mArrayLayerCount + layer]) {
                    SubresourceRange range = GetFullLayerRange(aspect, layer);
                    iterateFunc(range, &mData[layerDataOffset]);
                    continue;
                }

                for (uint32_t level = 0; level < mMipLevelCount; level++) {
                    SubresourceRange range =
                        SubresourceRange::SingleMipAndLayer(level, layer, aspect);
                    iterateFunc(range, &mData[layerDataOffset + level]);
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
        if (!mLayerCompressed[aspectIndex * mArrayLayerCount + arrayLayer]) {
            return mData[dataIndex];
        }

        return mData[dataIndex + mipLevel];
    }

    template <typename T>
    void SubresourceStorage<T>::DecompressAspect(uint32_t aspectIndex) {
        ASSERT(mAspectCompressed[aspectIndex]);

        size_t aspectDataOffset = aspectIndex * mArrayLayerCount * mMipLevelCount;
        for (uint32_t layer = 1; layer < mArrayLayerCount; layer++) {
            mData[aspectDataOffset + layer * mMipLevelCount] = mData[aspectDataOffset];
        }

        mAspectCompressed[aspectIndex] = false;
    }

    template <typename T>
    void SubresourceStorage<T>::RecompressAspect(uint32_t aspectIndex) {
        ASSERT(!mAspectCompressed[aspectIndex]);
        mAspectCompressed[aspectIndex] = true;

        size_t aspectDataOffset = aspectIndex * mArrayLayerCount * mMipLevelCount;
        for (uint32_t layer = 1; layer < mArrayLayerCount; layer++) {
            if (mData[aspectDataOffset + layer * mMipLevelCount] != mData[aspectDataOffset]) {
                mAspectCompressed[aspectIndex] = false;
                return;
            }
        }
    }

    template <typename T>
    void SubresourceStorage<T>::DecompressLayer(uint32_t aspectIndex, uint32_t layer) {
        ASSERT(mLayerCompressed[aspectIndex * mArrayLayerCount + layer]);

        size_t layerDataOffset = (aspectIndex * mArrayLayerCount + layer) * mMipLevelCount;
        for (uint32_t level = 1; level < mMipLevelCount; level++) {
            mData[layerDataOffset + level] = mData[layerDataOffset];
        }

        mLayerCompressed[aspectIndex * mArrayLayerCount + layer] = true;
    }

    template <typename T>
    void SubresourceStorage<T>::RecompressLayers(uint32_t aspectIndex, uint32_t layer) {
        ASSERT(mLayerCompressed[aspectIndex * mArrayLayerCount + layer]);
        mLayerCompressed[aspectIndex * mArrayLayerCount + layer] = true;

        size_t layerDataOffset = (aspectIndex * mArrayLayerCount + layer) * mMipLevelCount;
        for (uint32_t level = 1; level < mMipLevelCount; level++) {
            if (mData[layerDataOffset + level] != mData[layerDataOffset]) {
                mLayerCompressed[aspectIndex * mArrayLayerCount + layer] = false;
                return;
            }
        }
    }

    template <typename T>
    SubresourceRange SubresourceStorage<T>::GetFullAspectRange(Aspect aspect) const {
        SubresourceRange range;
        range.baseMipLevel = 0;
        range.levelCount = mMipLevelCount;
        range.baseArrayLayer = 0;
        range.layerCount = mArrayLayerCount;
        range.aspects = aspect;
        return range;
    }

    template <typename T>
    SubresourceRange SubresourceStorage<T>::GetFullLayerRange(Aspect aspect, uint32_t layer) const {
        SubresourceRange range;
        range.baseMipLevel = 0;
        range.levelCount = mMipLevelCount;
        range.baseArrayLayer = layer;
        range.layerCount = 1;
        range.aspects = aspect;
        return range;
    }

}  // namespace dawn_native

#endif  // DAWNNATIVE_SUBRESOURCESTORAGE_H_
