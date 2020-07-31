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
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/HeapD3D12.h"
#include "dawn_native/d3d12/ResidencyManagerD3D12.h"

namespace dawn_native { namespace d3d12 {

    HeapAllocator::HeapAllocator(Device* device,
                                 D3D12_HEAP_TYPE heapType,
                                 D3D12_HEAP_FLAGS heapFlags,
                                 MemorySegment memorySegment)
        : mDevice(device),
          mHeapType(heapType),
          mHeapFlags(heapFlags),
          mMemorySegment(memorySegment) {
    }

    ResultOrError<std::unique_ptr<ResourceHeapBase>> HeapAllocator::AllocateResourceHeap(
        uint64_t size) {
        std::unique_ptr<ResourceHeapBase> heapBase;
        // TODO(bryan.bernhart@intel.com): old resource heaps are evicted first. Consider LIFO?
        if (!mPool.empty() && mPool.front().heapSerial <= mDevice->GetCompletedCommandSerial()) {
            heapBase = std::move(mPool.front().heap);
            ASSERT(static_cast<Heap*>(heapBase.get())->GetSize() == size);
            mPool.pop_front();
        }

        if (heapBase == nullptr) {
            D3D12_HEAP_DESC heapDesc;
            heapDesc.SizeInBytes = size;
            heapDesc.Properties.Type = mHeapType;
            heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            heapDesc.Properties.CreationNodeMask = 0;
            heapDesc.Properties.VisibleNodeMask = 0;
            // It is preferred to use a size that is a multiple of the alignment.
            // However, MSAA heaps are always aligned to 4MB instead of 64KB. This means
            // if the heap size is too small, the VMM would fragment.
            // TODO(bryan.bernhart@intel.com): Consider having MSAA vs non-MSAA heaps.
            heapDesc.Alignment = D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
            heapDesc.Flags = mHeapFlags;

            // CreateHeap will implicitly make the created heap resident. We must ensure enough free
            // memory exists before allocating to avoid an out-of-memory error when overcommitted.
            DAWN_TRY(mDevice->GetResidencyManager()->EnsureCanAllocate(size, mMemorySegment));

            ComPtr<ID3D12Heap> d3d12Heap;
            DAWN_TRY(CheckOutOfMemoryHRESULT(
                mDevice->GetD3D12Device()->CreateHeap(&heapDesc, IID_PPV_ARGS(&d3d12Heap)),
                "ID3D12Device::CreateHeap"));

            heapBase = std::make_unique<Heap>(std::move(d3d12Heap), mMemorySegment, size);

            // Calling CreateHeap implicitly calls MakeResident on the new heap. We must track this
            // to avoid calling MakeResident a second time.
            mDevice->GetResidencyManager()->TrackResidentAllocation(ToBackend(heapBase.get()));
        }

        return std::move(heapBase);
    }

    void HeapAllocator::DeallocateResourceHeap(std::unique_ptr<ResourceHeapBase> heap) {
        // TODO(bryan.bernhart@intel.com): Consider periodically trimming to avoid OOM.
        mPool.push_back({mDevice->GetPendingCommandSerial(), std::move(heap)});
    }

    uint64_t HeapAllocator::GetPoolSizeForTesting() const {
        return mPool.size();
    }

}}  // namespace dawn_native::d3d12
