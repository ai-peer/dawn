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

#ifndef DAWNNATIVE_FORMAT_H_
#define DAWNNATIVE_FORMAT_H_

#include "dawn/native/dawn_platform.h"

#include "dawn/common/ityp_bitset.h"
#include "dawn/native/EnumClassBitmasks.h"
#include "dawn/native/Error.h"
#include "dawn/native/Subresource.h"

#include <array>

// About multi-planar formats.
//
// Dawn supports additional multi-planar formats when the multiplanar-formats extension is enabled.
// When enabled, Dawn treats planar data as sub-resources (ie. 1 sub-resource == 1 view == 1 plane).
// A multi-planar format name encodes the channel mapping and order of planes. For example,
// R8BG8Biplanar420Unorm is YUV 4:2:0 where Plane 0 = R8, and Plane 1 = BG8.
//
// Requirements:
// * Plane aspects cannot be combined with color, depth, or stencil aspects.
// * Only compatible multi-planar formats of planes can be used with multi-planar texture
// formats.
// * Can't access multiple planes without creating per plane views (no color conversion).
// * Multi-planar format cannot be written or read without a per plane view.
//
// TODO(dawn:551): Consider moving this comment.

namespace dawn::native {

    enum class Aspect : uint8_t;
    class DeviceBase;

    // This mirrors wgpu::TextureSampleType as a bitmask instead.
    enum class SampleTypeBit : uint8_t {
        None = 0x0,
        Float = 0x1,
        UnfilterableFloat = 0x2,
        Depth = 0x4,
        Sint = 0x8,
        Uint = 0x10,
    };

    // Converts an wgpu::TextureComponentType to its bitmask representation.
    SampleTypeBit ToSampleTypeBit(wgpu::TextureComponentType type);
    // Converts an wgpu::TextureSampleType to its bitmask representation.
    SampleTypeBit SampleTypeToSampleTypeBit(wgpu::TextureSampleType sampleType);

    struct TexelBlockInfo {
        uint32_t byteSize;
        uint32_t width;
        uint32_t height;
    };

    struct AspectInfo {
        TexelBlockInfo block;
        // TODO(crbug.com/dawn/367): Replace TextureComponentType with TextureSampleType, or make it
        // an internal Dawn enum.
        wgpu::TextureComponentType baseType;
        SampleTypeBit supportedSampleTypes;
        wgpu::TextureFormat format;
    };

    // The number of formats Dawn knows about. Asserts in BuildFormatTable ensure that this is the
    // exact number of known format.
    static constexpr size_t kKnownFormatCount = 96;

    struct Format;
    using FormatTable = std::array<Format, kKnownFormatCount>;

    // A wgpu::TextureFormat along with all the information about it necessary for validation.
    struct Format {
        wgpu::TextureFormat format;

        bool isRenderable;
        bool isCompressed;
        // A format can be known but not supported because it is part of a disabled extension.
        bool isSupported;
        bool supportsStorageUsage;
        bool supportsMultisample;
        bool supportsResolveTarget;
        Aspect aspects;
        // Only used for renderable color formats, number of color channels.
        uint8_t componentCount;

        bool IsColor() const;
        bool HasDepth() const;
        bool HasStencil() const;
        bool HasDepthOrStencil() const;

        // IsMultiPlanar() returns true if the format allows selecting a plane index. This is only
        // allowed by multi-planar formats (ex. NV12).
        bool IsMultiPlanar() const;

        const AspectInfo& GetAspectInfo(wgpu::TextureAspect aspect) const;
        const AspectInfo& GetAspectInfo(Aspect aspect) const;

        // The index of the format in the list of all known formats: a unique number for each format
        // in [0, kKnownFormatCount)
        size_t GetIndex() const;

        // baseFormat represents the memory layout of the format.
        // If two formats has the same baseFormat, they could copy to and be viewed as the other
        // format. Currently two formats have the same baseFormat if they differ only in sRGB-ness.
        wgpu::TextureFormat baseFormat;

        // Returns true if the formats are copy compatible.
        // Currently means they differ only in sRGB-ness.
        bool CopyCompatibleWith(const Format& format) const;

        // Returns true if the formats are texture view format compatible.
        // Currently means they differ only in sRGB-ness.
        bool ViewCompatibleWith(const Format& format) const;

      private:
        // Used to store the aspectInfo for one or more planes. For single plane "color" formats,
        // only the first aspect info or aspectInfo[0] is valid. For depth-stencil, the first aspect
        // info is depth and the second aspect info is stencil. For multi-planar formats,
        // aspectInfo[i] is the ith plane.
        std::array<AspectInfo, kMaxPlanesPerFormat> aspectInfo;

        friend FormatTable BuildFormatTable(const DeviceBase* device);
    };

    class FormatSetIterator {
        using IndexIterator = BitSetIterator<kKnownFormatCount, size_t>::Iterator;

      public:
        FormatSetIterator(const FormatTable& formatTable,
                          const std::bitset<kKnownFormatCount>& formatSet);

        class Iterator final {
          public:
            Iterator(const FormatTable& formatTable, IndexIterator&& iter);
            Iterator& operator++();
            bool operator==(const Iterator& other) const;
            bool operator!=(const Iterator& other) const;

            const Format& operator*();

          private:
            const FormatTable& mFormatTable;
            IndexIterator mIter;
        };

        Iterator begin() const;
        Iterator end() const;

      private:
        const FormatTable& mFormatTable;
        const std::bitset<kKnownFormatCount>& mFormatSet;
    };

    class FormatSet {
      public:
        void Set(const Format& format, bool value = true);
        bool Has(const Format& format) const;
        bool Any() const;

        friend FormatSetIterator MakeFormatSetIterator(const FormatTable& formatTable,
                                                       const FormatSet& formatSet);

      private:
        std::bitset<kKnownFormatCount> mFormatSet = {};
    };

    // Implementation details of the format table in the device.

    // Returns the index of a format in the FormatTable.
    size_t ComputeFormatIndex(wgpu::TextureFormat format);
    // Builds the format table with the extensions enabled on the device.
    FormatTable BuildFormatTable(const DeviceBase* device);

}  // namespace dawn::native

namespace dawn {

    template <>
    struct IsDawnBitmask<dawn::native::SampleTypeBit> {
        static constexpr bool enable = true;
    };

}  // namespace dawn

#endif  // DAWNNATIVE_FORMAT_H_
