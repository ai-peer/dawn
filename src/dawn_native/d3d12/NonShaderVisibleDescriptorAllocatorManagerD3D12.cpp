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

#include "dawn_native/d3d12/NonShaderVisibleDescriptorAllocatorManagerD3D12.h"

namespace dawn_native { namespace d3d12 {

    // TODO(dawn:155): Figure out this value.
    static constexpr uint16_t kDescriptorHeapSize = 1024;

    NonShaderVisibleDescriptorAllocatorManager::NonShaderVisibleDescriptorAllocatorManager(
        Device* device) {
        for (uint32_t countIndex = 1; countIndex <= kMaxBindingsPerGroup; countIndex++) {
            mViewAllocators[countIndex] = std::make_unique<NonShaderVisibleDescriptorAllocator>(
                device, countIndex, kDescriptorHeapSize, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            mSamplerAllocators[countIndex] = std::make_unique<NonShaderVisibleDescriptorAllocator>(
                device, countIndex, kDescriptorHeapSize, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        }
    }

    NonShaderVisibleDescriptorAllocator*
    NonShaderVisibleDescriptorAllocatorManager::GetViewAllocator(uint32_t descriptorCount) const {
        ASSERT(descriptorCount <= kMaxBindingsPerGroup);
        return mViewAllocators[descriptorCount].get();
    }

    NonShaderVisibleDescriptorAllocator*
    NonShaderVisibleDescriptorAllocatorManager::GetSamplerAllocator(
        uint32_t descriptorCount) const {
        ASSERT(descriptorCount <= kMaxBindingsPerGroup);
        return mSamplerAllocators[descriptorCount].get();
    }

}}  // namespace dawn_native::d3d12