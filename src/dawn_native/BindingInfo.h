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

#ifndef DAWNNATIVE_BINDINGINFO_H_
#define DAWNNATIVE_BINDINGINFO_H_

#include "common/Constants.h"
#include "common/TypedInteger.h"
#include "common/ityp_array.h"
#include "dawn_native/Format.h"
#include "dawn_native/PerStage.h"
#include "dawn_native/dawn_platform.h"

#include <cstdint>

namespace dawn_native {

    // Binding numbers in the shader and BindGroup/BindGroupLayoutDescriptors
    using BindingNumber = TypedInteger<struct BindingNumberT, uint32_t>;

    // Binding numbers get mapped to a packed range of indices
    using BindingIndex = TypedInteger<struct BindingIndexT, uint32_t>;

    using BindGroupIndex = TypedInteger<struct BindGroupIndexT, uint32_t>;

    static constexpr BindingIndex kMaxBindingsPerGroupTyped = BindingIndex(kMaxBindingsPerGroup);
    static constexpr BindGroupIndex kMaxBindGroupsTyped = BindGroupIndex(kMaxBindGroups);

    struct BindingInfo {
        BindingNumber binding;
        wgpu::ShaderStage visibility;
        wgpu::BindingType type;
        Format::Type textureComponentType = Format::Type::Float;
        wgpu::TextureViewDimension viewDimension = wgpu::TextureViewDimension::Undefined;
        wgpu::TextureFormat storageTextureFormat = wgpu::TextureFormat::Undefined;
        bool hasDynamicOffset = false;
        bool multisampled = false;
        uint64_t minBufferBindingSize = 0;
    };

    struct PerStageBindingCounts {
        uint32_t sampledTextureCount;
        uint32_t samplerCount;
        uint32_t storageBufferCount;
        uint32_t storageTextureCount;
        uint32_t uniformBufferCount;
    };

    struct BindingCounts {
        uint32_t totalCount;
        uint32_t bufferCount;
        uint32_t unverifiedBufferCount;  // Buffers with minimum buffer size unspecified
        uint32_t dynamicUniformBufferCount;
        uint32_t dynamicStorageBufferCount;
        PerStage<PerStageBindingCounts> perStage;
    };

    // For buffer size validation
    using RequiredBufferSizes = ityp::array<BindGroupIndex, std::vector<uint64_t>, kMaxBindGroups>;

}  // namespace dawn_native

#endif  // DAWNNATIVE_BINDINGINFO_H_
