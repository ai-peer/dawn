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

#include "dawn_native/vulkan/BufferUploader.h"

#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/RingBufferVk.h"

namespace dawn_native { namespace vulkan {

    BufferUploader::BufferUploader(Device* device, size_t initSize) : mDevice(device) {
        CreateBuffer(initSize);
    }

    void BufferUploader::CreateBuffer(size_t size) {
        mRingBuffers.emplace_back(std::make_unique<RingBuffer>(size, mDevice));
    }

    void BufferUploader::BufferSubData(VkBuffer buffer,
                                       VkDeviceSize offset,
                                       VkDeviceSize size,
                                       const void* data) {
        // Write to the staging buffer
        UploadHandle uploadHandle = Allocate(static_cast<size_t>(size), kDefaultAlignment);

        ASSERT(uploadHandle.mappedBuffer != nullptr);
        memcpy(uploadHandle.mappedBuffer, data, static_cast<size_t>(size));

        // Enqueue host write -> transfer src barrier and copy command
        VkCommandBuffer commands = mDevice->GetPendingCommandBuffer();

        VkMemoryBarrier barrier;
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        mDevice->fn.CmdPipelineBarrier(commands, VK_PIPELINE_STAGE_HOST_BIT,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0, nullptr,
                                       0, nullptr);

        const RingBuffer* ringBuffer = static_cast<RingBuffer*>(GetBuffer().get());

        VkBufferCopy copy;
        copy.srcOffset = uploadHandle.startOffset;
        copy.dstOffset = offset;
        copy.size = size;
        mDevice->fn.CmdCopyBuffer(commands, ringBuffer->GetBuffer(), buffer, 1, &copy);
    }

    void BufferUploader::Tick(Serial completedSerial) {
        DynamicUploader::Tick(completedSerial);
    }

}}  // namespace dawn_native::vulkan
