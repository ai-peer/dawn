// Copyright 2018 The Dawn Authors
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

#include "dawn_native/vulkan/MemoryHeapVk.h"
#include "dawn_native/vulkan/DeviceVk.h"

namespace dawn_native { namespace vulkan {

    MemoryHeap::MemoryHeap(Device* device,
                           VkDeviceMemory memory,
                           size_t size,
                           uint32_t heapTypeIndex)
        : ResourceHeapBase(size, heapTypeIndex), mDevice(device), mMemory(memory) {
    }

    MemoryHeap::~MemoryHeap() {
        // Allocation has not unmapped this resource.
        ASSERT(mMapRefCount == 0);
    }

    ResultOrError<void*> MemoryHeap::Map() {
        if (mMapRefCount == 0 && mDevice->fn.MapMemory(mDevice->GetVkDevice(), mMemory, 0, mSize, 0,
                                                       &mMappedPointer) != VK_SUCCESS) {
            return DAWN_CONTEXT_LOST_ERROR("Unable to map resource.");
        }
        mMapRefCount += 1;
        return mMappedPointer;
    }

    MaybeError MemoryHeap::Unmap() {
        if (mMapRefCount == 0) {
            return DAWN_CONTEXT_LOST_ERROR("Cannot unmap a resource that was never mapped.");
        }

        mMapRefCount--;

        if (mMapRefCount == 0) {
            ASSERT(mMemory != VK_NULL_HANDLE && mMappedPointer != nullptr);
            mDevice->fn.UnmapMemory(mDevice->GetVkDevice(), mMemory);
            mMappedPointer = nullptr;
        }

        return {};
    }

    VkDeviceMemory MemoryHeap::GetMemory() const {
        return mMemory;
    }

}}  // namespace dawn_native::vulkan