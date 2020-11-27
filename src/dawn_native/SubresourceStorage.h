#include "dawn_native/Texture.h"

#define kMaxPlanes 2

namespace dawn_native {

    uint8_t GetPlaneIndex(Aspect aspect) {
        ASSERT(HasOneBit(aspect));
        switch (aspect) {
            case Aspect::Color:
                return 0;
            case Aspect::Depth:
                return 0;
            case Aspect::Stencil:
                return 1;
            default:
                UNREACHABLE();
        }
    }

    uint8_t GetPlaneCount(Aspect aspects) {
        switch (aspect) {
            case Aspect::Color:
                return 1;
            case Aspect::Depth:
                return 1;
            case Aspect::Depth | Aspect::Stencil:
                return 2;
            default:
                UNREACHABLE();
        }
    }

    template <typename T>
    struct SubresourceStorage2 {
        SubresourseStorage(T defaultValue,
                           Aspect aspects,
                           uint32_t arrayLayerCount_,
                           uint32_t mipLevelCount_)
            : mipLevelCount(mipLevelCount_),
              planeCount(GetPlaneCount(aspects)),
              arrayLayerCount(arrayLayerCount_),
              planeCompressed{true, true},
              arrayLayerCompressed(arrayLayerCount * planeCount, false),
              data(planeCount * arrayLayerCount * mipLevelCount) {
            data[0] = defaultValue;
        }
        uint8_t mipLevelCount;
        uint8_t planeCount;
        uint16_t arrayLayerCount;

        std::array<bool, kMaxPlanes> mPlaneCompressed;
        std::vector<bool> mLayerCompressed;
        std::vector<T> data;

        template <typename F>
        void Update(const SubresourceRange& range, F&& updateFunc) {
            bool fullLayer = range.baseMipLevel == 0 && range.mipLevelCount == mipLevelCount;
            bool fullPlane =
                range.baseArrayLayer == 0 && range.arrayLayerCount == arrayLayerCount && fullMips;

            for (Aspect aspect : IterateEnumMask(range.aspects)) {
                uint32_t plane = GetPlaneIndex(aspect);
                uint32_t planeDataOffset = plane * arrayLayerCount * mipLevelCount;

                if (mPlaneCoyympressed[plane]) {
                    // Super happy case, we can call update once for the whole plane.
                    if (fullPlane) {
                        SubresourceRange udpateRange;
                        range.baseMipLevel = 0;
                        range.mipLevelCount = mipLevelCount;
                        range.baseArrayLayer = 0;
                        range.arrayLayerCount = arrayLayerCount;
                        range.aspect = aspect;
                        Update(updateRange, &data[planeDataOffset]);
                        continue;
                    }

                    // Decompress from per-plane to per layer
                    for (uint32_t layer = 1; layer < arrayLayerCount; layer++) {
                        data[planeDataOffset + layer * mipLevelCount] = data[planeDataOffset];
                    }
                }

                uint32_t layerEnd = range.baseArrayLayer + range.arrayLayerCount;
                for (uint32_t layer = range.baseArrayLayer, layer < layerEnd; layer++) {
                    uint32_t layerDataOffset = planeDataOffset + layer * mipLevelCount;

                    if (mLayerCompressed[planeCount * arrayLayerCount]) {
                        // Semi happy case, we can call update once for that layer.
                        if (fullLayer) {
                            SubresourceRange udpateRange;
                            range.baseMipLevel = 0;
                            range.mipLevelCount = mipLevelCount;
                            range.baseArrayLayer = layer;
                            range.arrayLayerCount = 1;
                            range.aspect = aspect;

                            Update(updateRange, &data[layerDataOffset]);
                            continue;
                        }

                        // Decompress from per-layer to per mip
                        for (uint32_t level = 1; level < mipLevelCount; level++) {
                            data[layerDataOffset + level] = data[layerDataOffset];
                        }
                    }

                    uint32_t levelEnd = range.baseMipLevel + range.mipLevelCount;
                    for (uint32_t level = range.baseMipLevel, level < levelEnd; level++) {
                        SubresourceRange udpateRange;
                        range.baseMipLevel = level;
                        range.mipLevelCount = 1;
                        range.baseArrayLayer = layer;
                        range.arrayLayerCount = 1;
                        range.aspect = aspect;
                        Update(updateRange, &data[level + layerDataOffset]);
                    }
                }

                // If we are fullLayer, then it is very likely that the the layers are compressed
                // after the update.
                if (fullLayer) {
                    uint32_t layerEnd = range.baseArrayLayer + range.arrayLayerCount;
                    for (uint32_t layer = range.baseArrayLayer, layer < layerEnd; layer++) {
                        if (mLayerCompressed[planeCount * arrayLayerCount]) {
                            continue;
                        }
                        mLayerCompressed[planeCount * arrayLayerCount] = true;

                        uint32_t layerDataOffset = planeDataOffset + layer * mipLevelCount;
                        for (uint32_t level = 1; level < mipLevelCount; level++) {
                            if (data[layerDataOffset] != data[layerDataOffset + level]) {
                                mLayerCompressed[planeCount * arrayLayerCount] = true;
                                break;
                            }
                        }
                    }
                }

                // If we are fullPlane then it is very likely that the plane is compressed after the
                // update.
                if (fullPlane) {
                    mPlaneCompressed[plane] = true;

                    for (uint32_t layer = 1; layer < arrayLayerCount; layer++) {
                        if (!mLayerCompressed[planeCount * arrayLayerCount] ||
                            data[planeDataOffset] !=
                                data[planeDataOffset + layer * mipLevelCount]) {
                            mPlaneCompressed[plane] = false;
                            break;
                        }
                    }
                }
            }
        }

        template <typename U, typename F>
        void Merge(const SubresourceStorage<U>& other, F&& mergeFunc) {
        }

        const T& GetSubresource(Aspect aspect, uint32_t arrayLayer, uint32_t mipLevel) {
            uint32_t layer = GetPlaneIndex(aspect);
            uint32_t dataIndex = layer * mipLevelCount * arrayLayerCount;

            if (!mPlaneCompressed[plane]) {
                dataIndex += arrayLayer * mipLevelCount;
            }
            if (!mLayerCompressed[layer * arrayLayerCount]) {
                dataIndex += mipLevel;
            }

            return data[dataIndex];
        }
    };

}  // namespace dawn_native
