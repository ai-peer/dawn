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

    template<typename T>
    struct SubresourceStorage2 {
        SubresourseStorage(T defaultValue, Aspect aspects, uint32_t arrayLayerCount_, uint32_t mipLevelCount_)
            : mipLevelCount(mipLevelCount_),
              planeCount(GetPlaneCount(aspects)),
              arrayLayerCount(arrayLayerCount_),
              planeCompressed{true, true},
              arrayLayerCompressed(arrayLayerCount * planeCount, false),
              data(planeCount * arrayLayerCount *mipLevelCount) {
            data[0] = defaultValue;
        }
        uint8_t mipLevelCount;
        uint8_t planeCount;
        uint16_t arrayLayerCount;

        std::array<bool, kMaxPlanes> planeCompressed;
        std::vector<bool> arrayLayerCompressed;
        std::vector<T> data;

        template<typename F>
        void Update(const SubresourceRange& range, F&& updateFunc) {
            bool fullMips = range.baseMipLevel == 0 && range.mipLevelCount == mipLevelCount;
            bool fullLayer = range.baseArrayLayer == 0 && range.arrayLayerCount == arrayLayerCount && fullMips;

            for (Aspect aspect : IterateEnumMask(range.aspects)) {
                uint32_t plane = GetPlaneIndex(aspect);

                SubresourceRange updateRange;
                updateRange.aspects = aspect;

                if (fillMips && planeCompressed[plane]) {
                    updateRange.baseMipLevel = range.baseMipLevel;
                    updateRange.levelCount = range.levelCount;
                    updateRange.baseArrayLayer = range.baseArrayLayer;
                    updateRange.arrayLayerCount = range.arrayLayerCount;
                    
                    uint32_t dataIndex = plane * arrayLayerCount * mipLevelCount;
                    T newData = Update(updateRange, data[dataIndex]);
                    data[dataIndex] = newData;
                    continue;
                }

                // TODO track if range all the same.
                updateRange.arrayLayerCount = 1;
                uint32_t layerEnd = range.baseArrayLayer + range.arrayLayerCount;
                for (uint32_t layer = range.baseArrayLayer, layer < layerEnd; layer ++) {
                    updateRange.baseArrayLayer = arrayLayer;

                    if (fullLayer && arrayLayerCompressed[plane * arrayLayerCount + layer]) {
                        
                        
                        continue;
                    }

                    uint32_t levelEnd = range.baseMipLevel + range.levelCount;
                    for (uint32_t level = range.baseMipLevel, level < levelEnd; level++) {
                    }
                }
            }
        }

        template<typename U, typename F>
        void Merge(const SubresourceStorage<U>& other, F&& mergeFunc) {
        }

        const T& GetSubresource(Aspect aspect, uint32_t arrayLayer, uint32_t mipLevel) {
            uint32_t layer = GetPlaneIndex(aspect);
            uint32_t dataIndex = layer * mipLevelCount * arrayLayerCount;

            if (!planeCompressed[plane]) {
                dataIndex += arrayLayer * mipLevelCount;
            }
            if (!arrayLayerCompressed[layer * arrayLayerCount]) {
                dataIndex += mipLevel;
            }

            return data[dataIndex];
        }
    };

}  // namespace dawn_native
