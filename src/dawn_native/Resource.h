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

#ifndef DAWNNATIVE_RESOURCE_H_
#define DAWNNATIVE_RESOURCE_H_

#include "dawn_native/Error.h"

namespace dawn_native {

    // Allocation method determines how memory was sub-divided.
    // Used by the device to get the allocator that was responsible for the allocation.
    enum class AllocationMethod {

        // Memory sub-divided using a single block of equal size.
        kDirect,

        // Memory sub-divided using one or more blocks of various sizes.
        kSubAllocated
    };

    // Wrapper for a resource.
    class ResourceBase {
      public:
        uint64_t GetOffset() const;
        AllocationMethod GetAllocationMethod() const;
        virtual ~ResourceBase() = default;

      protected:
        ResourceBase(uint64_t offset, AllocationMethod method);

        AllocationMethod mMethod;
        uint64_t mOffset;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_RESOURCE_H_