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

#ifndef DAWNNATIVE_VULKAN_RINGBUFFERVK_H_
#define DAWNNATIVE_VULKAN_RINGBUFFERVK_H_

#include "dawn_native/RingBuffer.h"
#include "dawn_native/vulkan/MemoryAllocator.h"

namespace dawn_native { namespace vulkan {

    class Device;
    class DeviceMemoryAllocation;

    class RingBuffer : public RingBufferBase {
      public:
        RingBuffer(size_t maxSize, Device* device);
        ~RingBuffer();

        VkBuffer GetBuffer() const {
            return mBuffer;
        }

      private:
        Serial GetPendingCommandSerial() const override;
        uint8_t* GetCPUVirtualAddressPointer() const override;

        Device* mDevice;
        uint8_t* mCpuVirtualAddress;
        VkBuffer mBuffer;
        DeviceMemoryAllocation mAllocation;
    };

}}  // namespace dawn_native::vulkan

#endif  // DAWNNATIVE_VULKAN_DEVICEVK_H_
