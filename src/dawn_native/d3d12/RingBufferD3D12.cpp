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

#include "dawn_native/d3d12/RingBufferD3D12.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/ResourceAllocator.h"

namespace dawn_native { namespace d3d12 {

    RingBuffer::RingBuffer(size_t maxSize, Device* device)
        : RingBufferBase(maxSize), mDevice(device) {
        D3D12_RESOURCE_DESC resourceDescriptor;
        resourceDescriptor.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDescriptor.Alignment = 0;
        resourceDescriptor.Width = maxSize;
        resourceDescriptor.Height = 1;
        resourceDescriptor.DepthOrArraySize = 1;
        resourceDescriptor.MipLevels = 1;
        resourceDescriptor.Format = DXGI_FORMAT_UNKNOWN;
        resourceDescriptor.SampleDesc.Count = 1;
        resourceDescriptor.SampleDesc.Quality = 0;
        resourceDescriptor.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDescriptor.Flags = D3D12_RESOURCE_FLAG_NONE;

        mResource = mDevice->GetResourceAllocator()->Allocate(
            D3D12_HEAP_TYPE_UPLOAD, resourceDescriptor, D3D12_RESOURCE_STATE_GENERIC_READ);

        // TODO(b-brber): Record the GPU pointer for generic non-upload usage.

        ASSERT_SUCCESS(mResource->Map(0, nullptr, &mCpuVirtualAddress));
    }

    RingBuffer::~RingBuffer() {
        // Invalidate the CPU virtual address & flush cache (if needed).
        mResource->Unmap(0, nullptr);
        mCpuVirtualAddress = nullptr;

        mDevice->GetResourceAllocator()->Release(mResource);
    }

    Serial RingBuffer::GetPendingCommandSerial() const {
        return mDevice->GetPendingCommandSerial();
    }

    uint8_t* RingBuffer::GetCPUVirtualAddressPointer() const {
        ASSERT(mCpuVirtualAddress != nullptr);
        return static_cast<uint8_t*>(mCpuVirtualAddress);
    }

}}  // namespace dawn_native::d3d12