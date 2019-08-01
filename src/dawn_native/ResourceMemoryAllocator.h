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

#ifndef DAWNNATIVE_RESOURCEMEMORYALLOCATOR_H_
#define DAWNNATIVE_RESOURCEMEMORYALLOCATOR_H_

#include <memory>
#include <vector>

#include "dawn_native/Error.h"
#include "dawn_native/ResourceMemoryAllocation.h"

namespace dawn_native {
    //
    // Resource allocator overview.
    //
    // There are two supported modes of allocation: sub-allocation or not (otherwise called direct
    // allocation). Sub-allocation sub-divides a larger memory space enabling memory reuse while
    // direct allocation allocates memory sized to the allocation request.
    //
    // Front-end vs Back-end
    //
    // This class uses a nested allocator design. The innermost allocator allocates device memory
    // or a "resource heap". While the outtermost allocator "allocates" an offset to be used by the
    // API using the memory pool handle.
    //
    // Lifetime management
    //
    // In sub-allocation, the sub-allocator controls the lifetime of the resource heap since the
    // same resource heap can be sub-allocated with one or more resources. When using direct
    // allocation, the client controls the lifetime. In either case, the client is always
    // responsible to call Deallocate to avoid leaking memory.

    static constexpr uint64_t kInvalidOffset = std::numeric_limits<uint64_t>::max();

    // Common interface for resource memory created by the device.
    class ResourceMemoryAllocator {
      public:
        ResourceMemoryAllocator() = default;
        virtual ~ResourceMemoryAllocator() = default;

        virtual ResultOrError<ResourceMemoryAllocation> Allocate(uint64_t allocationSize,
                                                                 uint64_t alignment = 0,
                                                                 int memoryFlags = 0) = 0;

        virtual void Deallocate(ResourceMemoryAllocation allocation) = 0;
        virtual void Tick(uint64_t lastCompletedSerial) = 0;
    };
}  // namespace dawn_native

#endif  // DAWNNATIVE_RESOURCEMEMORYALLOCATOR_H_
