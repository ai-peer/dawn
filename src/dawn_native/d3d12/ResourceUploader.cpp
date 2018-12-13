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

#include "dawn_native/d3d12/ResourceUploader.h"

#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/RingBufferD3D12.h"

namespace dawn_native { namespace d3d12 {

    ResourceUploader::ResourceUploader(Device* device, size_t initSize) : mDevice(device) {
        CreateBuffer(initSize);
    }

    void ResourceUploader::CreateBuffer(size_t size) {
        mRingBuffers.emplace_back(std::make_unique<RingBuffer>(size, mDevice));
    }

    void ResourceUploader::BufferSubData(ComPtr<ID3D12Resource> resource,
                                         uint32_t start,
                                         uint32_t count,
                                         const void* data) {
        UploadHandle uploadHandle = Allocate(count, kDefaultAlignment);
        ASSERT(uploadHandle.mappedBuffer != nullptr);

        memcpy(uploadHandle.mappedBuffer, data, count);
        const RingBuffer* ringBuffer = static_cast<RingBuffer*>(GetBuffer().get());

        mDevice->GetPendingCommandList()->CopyBufferRegion(
            resource.Get(), start, ringBuffer->GetResource(), uploadHandle.startOffset, count);
    }

}}  // namespace dawn_native::d3d12
