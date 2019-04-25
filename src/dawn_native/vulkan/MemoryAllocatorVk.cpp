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

#include "dawn_native/vulkan/MemoryAllocatorVk.h"

#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/FencedDeleter.h"
#include "dawn_native/vulkan/MemoryHeapVk.h"

namespace dawn_native { namespace vulkan {

    MemoryAllocator2::MemoryAllocator2(Device* device, uint32_t heapTypeIndex)
        : mDevice(device), mHeapTypeIndex(heapTypeIndex) {
    }

    ResourceHeapBase* MemoryAllocator2::Allocate(size_t heapSize) {
        VkMemoryAllocateInfo allocateInfo;
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.pNext = nullptr;
        allocateInfo.allocationSize = heapSize;
        allocateInfo.memoryTypeIndex = static_cast<uint32_t>(mHeapTypeIndex);

        VkDeviceMemory allocatedMemory = VK_NULL_HANDLE;
        if (mDevice->fn.AllocateMemory(mDevice->GetVkDevice(), &allocateInfo, nullptr,
                                       &allocatedMemory) != VK_SUCCESS) {
            return nullptr;
        }

#if defined(_DEBUG)
        mAllocationCount++;
#endif

        return new MemoryHeap(mDevice, allocatedMemory, heapSize, mHeapTypeIndex);
    }

    void MemoryAllocator2::Deallocate(ResourceHeapBase* heap) {
#if defined(_DEBUG)
        if (mAllocationCount == 0) {
            ASSERT(false);
        }
        mAllocationCount--;
#endif
        mDevice->GetFencedDeleter()->DeleteWhenUnused(ToBackend(heap)->GetMemory());

        delete heap;
    }

    void MemoryAllocator2::Tick(uint64_t lastCompletedSerial) {
    }

}}  // namespace dawn_native::vulkan