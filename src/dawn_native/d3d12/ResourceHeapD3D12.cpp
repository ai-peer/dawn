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

#include "dawn_native/d3d12/ResourceHeapD3D12.h"
#include "dawn_native/d3d12/DeviceD3D12.h"

namespace dawn_native { namespace d3d12 {

    ResourceHeap::ResourceHeap(ComPtr<ID3D12Heap> heap, D3D12_HEAP_TYPE heapType)
        : mHeap(heap), mHeapType(heapType) {
    }

    ResourceHeap::ResourceHeap(ComPtr<ID3D12Resource> resource,
                               ComPtr<ID3D12Heap> heap,
                               D3D12_HEAP_TYPE heapType)
        : mResource(resource), mHeap(heap), mHeapType(heapType) {
    }

    ComPtr<ID3D12Resource> ResourceHeap::GetD3D12Resource() const {
        return mResource;
    }

    ComPtr<ID3D12Heap> ResourceHeap::GetD3D12Heap() const {
        return mHeap;
    }

    D3D12_HEAP_TYPE ResourceHeap::GetD3D12HeapType() const {
        return mHeapType;
    }

    MaybeError ResourceHeap::MapImpl() {
        if (FAILED(mResource->Map(0, nullptr, &mMappedPointer))) {
            return DAWN_CONTEXT_LOST_ERROR("Unable to map resource.");
        }
        return {};
    }

    void ResourceHeap::UnmapImpl() {
        // Invalidates CPU virtual address & flushes cache (if needed).
        mResource->Unmap(0, nullptr);
    }

    D3D12_GPU_VIRTUAL_ADDRESS ResourceHeap::GetGPUPointer() const {
        return mResource->GetGPUVirtualAddress();
    }
}}  // namespace dawn_native::d3d12