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

#include "dawn_native/ResourceHeapAllocator.h"
#include "dawn_native/ResourceHeap.h"

#include "common/Math.h"

namespace dawn_native {

    static constexpr uint64_t INVALID_OFFSET = std::numeric_limits<uint64_t>::max();

    // DirectResourceMemoryAllocator

    template <class TResourceHeapAllocator>
    template <typename... ResourceAllocatorArgs>
    DirectResourceMemoryAllocator<TResourceHeapAllocator>::DirectResourceMemoryAllocator(
        ResourceAllocatorArgs&&... resourceCreationArgs)
        : mResourceHeapAllocator(std::forward<ResourceAllocatorArgs>(resourceCreationArgs)...) {
    }

    template <class TResourceHeapAllocator>
    ResourceMemoryAllocation DirectResourceMemoryAllocator<TResourceHeapAllocator>::Allocate(
        uint64_t allocationSize,
        uint64_t allocationAlignment,
        int memoryFlags) {
        std::unique_ptr<ResourceHeapBase> newResourceHeap(
            mResourceHeapAllocator.CreateHeap(allocationSize, memoryFlags));

        if (newResourceHeap == nullptr) {
            return ResourceMemoryAllocation{INVALID_OFFSET};
        };

        return ResourceMemoryAllocation{/*offset*/ 0, newResourceHeap.release(), /*IsDirect*/ true};
    }

    template <class TResourceHeapAllocator>
    void DirectResourceMemoryAllocator<TResourceHeapAllocator>::Deallocate(
        ResourceMemoryAllocation allocation) {
        mResourceHeapAllocator.FreeHeap(allocation.GetResourceHeap());
    }

    template <class TResourceHeapAllocator>
    void DirectResourceMemoryAllocator<TResourceHeapAllocator>::Tick(uint64_t lastCompletedSerial) {
        mResourceHeapAllocator.Tick(lastCompletedSerial);
    }
}  // namespace dawn_native