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

#include "dawn_native/ResourceMemoryAllocation.h"
#include "common/Assert.h"

#include <limits>

namespace dawn_native {

    ResourceMemoryAllocation::ResourceMemoryAllocation()
        : mOffset(0), mResource(nullptr), mMappedPointer(nullptr) {
    }

    ResourceMemoryAllocation::ResourceMemoryAllocation(AllocationInfo& info,
                                                       uint64_t offset,
                                                       ResourceBase* resource,
                                                       uint8_t* mappedPointer)
        : mInfo(info), mOffset(offset), mResource(resource), mMappedPointer(mappedPointer) {
    }

    ResourceBase* ResourceMemoryAllocation::GetResource() const {
        return mResource;
    }

    uint64_t ResourceMemoryAllocation::GetOffset() const {
        return mOffset;
    }

    uint8_t* ResourceMemoryAllocation::GetMappedPointer() const {
        return mMappedPointer;
    }

    AllocationInfo ResourceMemoryAllocation::GetInfo() const {
        return mInfo;
    }

    void ResourceMemoryAllocation::Invalidate() {
        mResource = nullptr;
        mInfo = {};
    }
}  // namespace dawn_native