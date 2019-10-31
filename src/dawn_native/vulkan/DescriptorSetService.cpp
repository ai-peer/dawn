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

#include "dawn_native/vulkan/DescriptorSetService.h"

#include "dawn_native/vulkan/BindGroupLayoutVk.h"
#include "dawn_native/vulkan/DeviceVk.h"

namespace dawn_native { namespace vulkan {

    DescriptorSetService::DescriptorSetService(Device* device) : mDevice(device) {
    }

    DescriptorSetService::~DescriptorSetService() {
        ASSERT(mDeallocations.Empty());
    }

    void DescriptorSetService::AddDeferredDeallocation(BindGroupLayout* layout, size_t index) {
        mDeallocations.Enqueue({layout, index}, mDevice->GetPendingCommandSerial());
    }

    void DescriptorSetService::Tick(Serial completedSerial) {
        for (const std::pair<BindGroupLayout*, size_t>& dealloc :
             mDeallocations.IterateUpTo(completedSerial)) {
            dealloc.first->FinishDeallocation(dealloc.second);
        }

        mDeallocations.ClearUpTo(completedSerial);
    }

}}  // namespace dawn_native::vulkan
