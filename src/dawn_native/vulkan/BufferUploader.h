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

#ifndef DAWNNATIVE_VULKAN_BUFFERUPLOADER_H_
#define DAWNNATIVE_VULKAN_BUFFERUPLOADER_H_

#include "common/SerialQueue.h"
#include "common/vulkan_platform.h"
#include "dawn_native/DynamicUploader.h"

namespace dawn_native { namespace vulkan {

    class Device;

    class BufferUploader : public DynamicUploader {
      public:
        BufferUploader(Device* device, size_t initSize = kBaseRingBufferSize);
        ~BufferUploader() = default;

        void BufferSubData(VkBuffer buffer,
                           VkDeviceSize offset,
                           VkDeviceSize size,
                           const void* data);

        void Tick(Serial completedSerial);

        void CreateBuffer(size_t size) override;

      private:
        static constexpr size_t kBaseRingBufferSize =
            64000;  // TODO(b-brber): Figure out these values.
        static constexpr size_t kDefaultAlignment = 4;

        Device* mDevice;
    };

}}  // namespace dawn_native::vulkan

#endif  // DAWNNATIVE_VULKAN_BUFFERUPLOADER_H_
