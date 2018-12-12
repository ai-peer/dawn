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

#ifndef DAWNNATIVE_D3D12_RESOURCEUPLOADER_H_
#define DAWNNATIVE_D3D12_RESOURCEUPLOADER_H_

#include "dawn_native/DynamicUploader.h"
#include "dawn_native/d3d12/d3d12_platform.h"

namespace dawn_native { namespace d3d12 {

    class Device;

    class ResourceUploader : public DynamicUploader {
      public:
        ResourceUploader(Device* device, size_t initSize = kBaseRingBufferSize);
        ~ResourceUploader() = default;

        void BufferSubData(ComPtr<ID3D12Resource> resource,
                           uint32_t start,
                           uint32_t count,
                           const void* data);

        void CreateBuffer(size_t size) override;

      private:
        static constexpr size_t kBaseRingBufferSize = 64000;  // DXGI min heap size is 64K.
        static constexpr size_t kDefaultAlignment =
            4;  // D3D does not specify so we assume 4-byte alignment to be safe.

        Device* mDevice;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_RESOURCEUPLOADER_H_
