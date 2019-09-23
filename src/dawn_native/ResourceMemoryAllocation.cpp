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

namespace dawn_native {

    ResourceMemoryAllocationBase::ResourceMemoryAllocationBase()
        : mMemoryOffset(0), mMappedPointer(nullptr) {
    }

    ResourceMemoryAllocationBase::ResourceMemoryAllocationBase(const AllocationInfo& info,
                                                               uint64_t offset,
                                                               uint8_t* mappedPointer)
        : mInfo(info), mMemoryOffset(offset), mMappedPointer(mappedPointer) {
    }

    uint64_t ResourceMemoryAllocationBase::GetOffset() const {
        return mMemoryOffset;
    }

    uint8_t* ResourceMemoryAllocationBase::GetMappedPointer() const {
        return mMappedPointer;
    }

    AllocationInfo ResourceMemoryAllocationBase::GetInfo() const {
        return mInfo;
    }

    void ResourceMemoryAllocationBase::Invalidate() {
        mInfo = {};
    }
}  // namespace dawn_native