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

#ifndef DAWNNATIVE_EXTERNALSLABALLOCATOR_H_
#define DAWNNATIVE_EXTERNALSLABALLOCATOR_H_

#include "dawn_native/Error.h"

#include <vector>

namespace dawn_native {

    template <typename Derived, typename Traits>
    class ExternalSlabAllocator {
      protected:
        using HeapIndex = typename Traits::HeapIndex;
        using AllocationIndex = typename Traits::AllocationIndex;
        using AllocationInfo = typename Traits::AllocationInfo;
        using Heap = typename Traits::Heap;

      public:
        ExternalSlabAllocator() = default;
        ~ExternalSlabAllocator() {
            for (HeapInfo& info : mHeapPool) {
                static_cast<Derived*>(this)->DeallocateHeapImpl(&info.heap);
            }
        }

        ResultOrError<AllocationInfo> Allocate() {
            if (mAvailableHeapIndices.empty()) {
                DAWN_TRY(AllocateHeap());
            }

            ASSERT(!mAvailableHeapIndices.empty());
            const HeapIndex heapIndex = mAvailableHeapIndices.back();
            HeapInfo& heapInfo = mHeapPool[heapIndex];

            ASSERT(!heapInfo.freeBlockIndices.empty());

            const auto allocationIndex = heapInfo.freeBlockIndices.back();

            AllocationInfo allocation;
            DAWN_TRY_ASSIGN(allocation, static_cast<Derived*>(this)->AllocateImpl(
                                            &heapInfo.heap, heapIndex, allocationIndex));

            heapInfo.freeBlockIndices.pop_back();
            return allocation;
        }

        void Deallocate(AllocationInfo* allocationInfo) {
            static_cast<Derived*>(this)->DeallocateImpl(allocationInfo);
        }

        void DidDeallocate(HeapIndex heapIndex, AllocationIndex allocationIndex) {
            auto& freeBlockIndices = mHeapPool[heapIndex].freeBlockIndices;
            if (freeBlockIndices.empty()) {
                mAvailableHeapIndices.emplace_back(heapIndex);
            }
            freeBlockIndices.emplace_back(allocationIndex);
        }

      private:
        struct HeapInfo {
            Heap heap;
            std::vector<AllocationIndex> freeBlockIndices;
        };

        MaybeError AllocateHeap() {
            Heap heap = {};
            AllocationIndex blockCount;
            DAWN_TRY_ASSIGN(std::tie(heap, blockCount),
                            static_cast<Derived*>(this)->AllocateHeapImpl());

            HeapInfo info = {
                std::move(heap),
                std::vector<AllocationIndex>(blockCount),
            };
            for (AllocationIndex blockIndex = 0; blockIndex < blockCount; ++blockIndex) {
                info.freeBlockIndices[blockIndex] = blockIndex;
            }

            mAvailableHeapIndices.push_back(mHeapPool.size());
            mHeapPool.emplace_back(std::move(info));

            return {};
        }

        std::vector<HeapIndex> mAvailableHeapIndices;
        std::vector<HeapInfo> mHeapPool;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_EXTERNALSLABALLOCATOR_H_
