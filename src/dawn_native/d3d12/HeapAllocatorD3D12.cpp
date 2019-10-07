// Copyright 2019 The Dawn Authors
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

#include "dawn_native/d3d12/HeapAllocatorD3D12.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/HeapD3D12.h"

namespace dawn_native { namespace d3d12 {

    HeapAllocator::HeapAllocator(Device* device, D3D12_HEAP_TYPE heapType)
        : mDevice(device), mHeapType(heapType) {
    }

    ResultOrError<std::unique_ptr<ResourceHeapBase>> HeapAllocator::Allocate(uint64_t size,
                                                                             int memoryFlags) {
        D3D12_HEAP_DESC heapDesc;
        heapDesc.SizeInBytes = size;
        heapDesc.Properties.Type = mHeapType;
        heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapDesc.Properties.CreationNodeMask = 0;
        heapDesc.Properties.VisibleNodeMask = 0;
        // MSAA vs non-MSAA resources have separate heap alignments.
        // TODO(bryan.bernhart@intel.com): Support heap creation containing MSAA resources.
        heapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        heapDesc.Flags = static_cast<D3D12_HEAP_FLAGS>(memoryFlags);

        ComPtr<ID3D12Heap> heap;
        if (FAILED(mDevice->GetD3D12Device()->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap)))) {
            return DAWN_OUT_OF_MEMORY_ERROR("Unable to allocate heap");
        }

        std::unique_ptr<ResourceHeapBase> heapMemory = std::make_unique<Heap>(std::move(heap));
        return heapMemory;
    }

    void HeapAllocator::Deallocate(std::unique_ptr<ResourceHeapBase> heap) {
        mDevice->ReferenceUntilUnused(static_cast<Heap*>(heap.get())->GetD3D12Heap());
    }

}}  // namespace dawn_native::d3d12