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

#ifndef DAWNNATIVE_D3D12_NONSHADERVISIBLEDESCRIPTORALLOCATOR_H_
#define DAWNNATIVE_D3D12_NONSHADERVISIBLEDESCRIPTORALLOCATOR_H_

#include "dawn_native/Error.h"

#include "dawn_native/d3d12/NonShaderVisibleHeapAllocationD3D12.h"

#include <list>
#include <vector>

// |NonShaderVisibleDescriptorAllocator| allocates a fixed-size block of descriptors from a CPU
// descriptor heap pool.
// Internally, it manages a list of heaps using a Simple List of Blocks (SLOB)
// allocator. The SLOB allocator only needs the raw offset and index of the heap in the pool. The
// SLOB is backed by a linked-list of blocks (free-list). The heap is in one of two states: free or
// NOT. To allocate, a block of the range [base + start, base + end] is removed from the free-list.
// A "free" heap always has room for at-least one block. Iff no free heap exists, a new heap is
// created and inserted back to the pool to be immediately used. To deallocate, the allocation is
// "freed" by inserting a block back into the free-list.
//
// The SLOB allocator uses a first-first algorithm. If it's a new heap, the free block start is
// bumped-up by the block size; otherwise, the entire free block is allocated from the heap.
// The downside to this simple strategy is blocks can become heavily fragmented after heap space is
// exhausted and more sparse de-allocation occurs.
namespace dawn_native { namespace d3d12 {

    class Device;

    class NonShaderVisibleDescriptorAllocator {
      public:
        NonShaderVisibleDescriptorAllocator() = default;
        NonShaderVisibleDescriptorAllocator(Device* device,
                                            uint32_t descriptorCount,
                                            uint32_t heapSize,
                                            D3D12_DESCRIPTOR_HEAP_TYPE heapType);
        ~NonShaderVisibleDescriptorAllocator() = default;

        ResultOrError<NonShaderVisibleHeapAllocation> AllocateCPUDescriptors();

        void Deallocate(D3D12_CPU_DESCRIPTOR_HANDLE& baseDescriptor, uint32_t heapIndex);

        uint32_t GetPoolSizeForTesting() const;

      private:
        struct HeapBlock {
            uint64_t start = 0;
            uint64_t end = 0;
        };

        struct NonShaderVisibleBuffer {
            ComPtr<ID3D12DescriptorHeap> heap;
            std::list<HeapBlock> freeList;
        };

        MaybeError AllocateCPUHeap();

        std::list<uint32_t> mFreeHeaps;
        std::vector<NonShaderVisibleBuffer> mPool;

        Device* mDevice;

        uint32_t mSizeIncrement;
        uint32_t mBlockSize;
        uint32_t mHeapSize;

        D3D12_DESCRIPTOR_HEAP_TYPE mHeapType;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_NONSHADERVISIBLEDESCRIPTORALLOCATOR_H_