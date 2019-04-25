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

#include "dawn_native/d3d12/ResourceAllocatorD3D12.h"

#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/ResourceHeapD3D12.h"

namespace dawn_native { namespace d3d12 {

    namespace {

        static constexpr D3D12_HEAP_PROPERTIES kDefaultHeapProperties = {
            D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0,
            0};

        static constexpr D3D12_HEAP_PROPERTIES kUploadHeapProperties = {
            D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0,
            0};

        static constexpr D3D12_HEAP_PROPERTIES kReadbackHeapProperties = {
            D3D12_HEAP_TYPE_READBACK, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0,
            0};
    }  // namespace

    ResourceAllocator2::ResourceAllocator2(Device* device, uint32_t heapTypeIndex)
        : mDevice(device), mHeapTypeIndex(heapTypeIndex) {
    }

    D3D12_RESOURCE_STATES ResourceAllocator2::GetHeapState(D3D12_HEAP_TYPE heapType) const {
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

        // D3D12 requires buffers on the READBACK heap to have the D3D12_RESOURCE_STATE_COPY_DEST
        // state
        if (heapType == D3D12_HEAP_TYPE_READBACK) {
            initialState |= D3D12_RESOURCE_STATE_COPY_DEST;
        }

        // D3D12 requires buffers on the UPLOAD heap to have the D3D12_RESOURCE_STATE_GENERIC_READ
        // state
        if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
            initialState |= D3D12_RESOURCE_STATE_GENERIC_READ;
        }

        return initialState;
    }

    D3D12_HEAP_PROPERTIES ResourceAllocator2::GetHeapProperties(D3D12_HEAP_TYPE heapType) const {
        switch (heapType) {
            case D3D12_HEAP_TYPE_UPLOAD:
                return kUploadHeapProperties;
            case D3D12_HEAP_TYPE_READBACK:
                return kReadbackHeapProperties;
            default:
                return kDefaultHeapProperties;
        }
    }

    ResourceHeapBase* ResourceAllocator2::Allocate(size_t heapSize) {
        // https://docs.microsoft.com/en-us/windows/desktop/direct3d12/cd3dx12-resource-desc
        D3D12_RESOURCE_DESC resourceDescriptor;
        resourceDescriptor.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDescriptor.Alignment = 0;
        resourceDescriptor.Width = heapSize;
        resourceDescriptor.Height = 1;
        resourceDescriptor.DepthOrArraySize = 1;
        resourceDescriptor.MipLevels = 1;
        resourceDescriptor.Format = DXGI_FORMAT_UNKNOWN;
        resourceDescriptor.SampleDesc.Count = 1;
        resourceDescriptor.SampleDesc.Quality = 0;
        resourceDescriptor.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDescriptor.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_TYPE heapType = mDevice->GetDeviceInfo().heapTypes[mHeapTypeIndex];
        D3D12_HEAP_PROPERTIES heapProperties = GetHeapProperties(heapType);

        ComPtr<ID3D12Resource> resource;
        if (FAILED(mDevice->GetD3D12Device()->CreateCommittedResource(
                &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDescriptor, GetHeapState(heapType),
                nullptr, IID_PPV_ARGS(&resource)))) {
            return nullptr;
        }

#if defined(_DEBUG)
        mAllocationCount++;
#endif

        return new ResourceHeap(resource, heapSize, mHeapTypeIndex);
    }

    void ResourceAllocator2::Deallocate(ResourceHeapBase* heap) {
#if defined(_DEBUG)
        if (mAllocationCount == 0) {
            ASSERT(false);
        }
        mAllocationCount--;
#endif
        // Resources may still be in use on the GPU. Enqueue them so that we hold onto them until
        // GPU execution has completed
        mReleasedResources.Enqueue(ToBackend(heap)->GetD3D12Resource(),
                                   mDevice->GetPendingCommandSerial());

        delete heap;
    }

    void ResourceAllocator2::Tick(uint64_t lastCompletedSerial) {
        mReleasedResources.ClearUpTo(lastCompletedSerial);
    }
}}  // namespace dawn_native::d3d12
