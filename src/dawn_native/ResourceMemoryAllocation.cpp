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

    ResourceMemoryAllocation::ResourceMemoryAllocation(uint64_t offset,
                                                       ResourceHeapBase* resourceHeap,
                                                       bool isDirect)
        : mIsDirect(isDirect), mOffset(offset), mResourceHeap(resourceHeap) {
    }

    ResourceHeapBase* ResourceMemoryAllocation::GetResourceHeap() const {
        return mResourceHeap;
    }

    uint64_t ResourceMemoryAllocation::GetOffset() const {
        return mOffset;
    }

    bool ResourceMemoryAllocation::IsDirect() const {
        return mIsDirect;
    }
}  // namespace dawn_native