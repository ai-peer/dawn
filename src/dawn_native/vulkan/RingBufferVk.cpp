// Copyright 2017 The Dawn Authors
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

#include "dawn_native/vulkan/RingBufferVk.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/FencedDeleter.h"
#include "dawn_native/vulkan/MemoryAllocator.h"

namespace dawn_native { namespace vulkan {

    RingBuffer::RingBuffer(size_t maxSize, Device* device)
        : RingBufferBase(maxSize), mDevice(device) {
        VkBufferCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.size = maxSize;
        createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = 0;

        if (mDevice->fn.CreateBuffer(mDevice->GetVkDevice(), &createInfo, nullptr, &mBuffer) !=
            VK_SUCCESS) {
            ASSERT(false);
        }

        VkMemoryRequirements requirements;
        mDevice->fn.GetBufferMemoryRequirements(mDevice->GetVkDevice(), mBuffer, &requirements);

        if (!mDevice->GetMemoryAllocator()->Allocate(requirements, true, &mAllocation)) {
            ASSERT(false);
        }

        if (mDevice->fn.BindBufferMemory(mDevice->GetVkDevice(), mBuffer, mAllocation.GetMemory(),
                                         mAllocation.GetMemoryOffset()) != VK_SUCCESS) {
            ASSERT(false);
        }

        mCpuVirtualAddress = mAllocation.GetMappedPointer();
        ASSERT(mCpuVirtualAddress != nullptr);
    }

    RingBuffer::~RingBuffer() {
        mCpuVirtualAddress = nullptr;
        mDevice->GetFencedDeleter()->DeleteWhenUnused(mBuffer);
        mDevice->GetMemoryAllocator()->Free(&mAllocation);
    }

    UploadHandle RingBuffer::SubAllocate(size_t allocSize) {
        size_t startOffset = SubAllocateBase(allocSize);

        UploadHandle uploadHandle;
        uploadHandle.mappedBuffer = nullptr;

        if (startOffset == static_cast<size_t>(-1))
            return uploadHandle;

        // Track new request when serial advances.
        const Serial currentSerial = mDevice->GetSerial();
        if (mInflightRequests.Empty() || currentSerial > mInflightRequests.LastSerial()) {
            Request request;
            request.endOffset = mUsedEndOffset;
            request.size = mCurrentRequestSize;

            mInflightRequests.Enqueue(std::move(request), currentSerial);
            mCurrentRequestSize = 0;  // reset
        }

        uploadHandle.mappedBuffer = reinterpret_cast<uint8_t*>(mCpuVirtualAddress) + startOffset;
        uploadHandle.startOffset = startOffset;

        return uploadHandle;
    }

}}  // namespace dawn_native::vulkan
