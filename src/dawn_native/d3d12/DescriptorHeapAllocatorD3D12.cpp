// Copyright 2020 The Dawn Authors
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

#include "dawn_native/d3d12/DescriptorHeapAllocatorD3D12.h"
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"

namespace dawn_native { namespace d3d12 {

    DescriptorHeapAllocator2::DescriptorHeapAllocator2(Device* device) : mDevice(device) {
    }

    ResultOrError<ComPtr<ID3D12DescriptorHeap>> DescriptorHeapAllocator2::AllocateDescriptorHeap(
        uint32_t heapSize,
        D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags,
        D3D12_DESCRIPTOR_HEAP_TYPE heapType) {
        D3D12_DESCRIPTOR_HEAP_DESC heapDescriptor;
        heapDescriptor.Type = heapType;
        heapDescriptor.NumDescriptors = heapSize;
        heapDescriptor.Flags = heapFlags;
        heapDescriptor.NodeMask = 0;
        ComPtr<ID3D12DescriptorHeap> heap;
        DAWN_TRY(CheckHRESULT(
            mDevice->GetD3D12Device()->CreateDescriptorHeap(&heapDescriptor, IID_PPV_ARGS(&heap)),
            "ID3D12Device::CreateDescriptorHeap"));
        return heap;
    }

    void DescriptorHeapAllocator2::DeallocateDescriptorHeap(ComPtr<ID3D12DescriptorHeap> heap) {
        mDevice->ReferenceUntilUnused(heap);
    }
}}  // namespace dawn_native::d3d12
