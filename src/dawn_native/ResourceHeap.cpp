// Copyright 2018 The Dawn Authors
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

#include "dawn_native/ResourceHeap.h"

namespace dawn_native {

    ResourceHeapBase::ResourceHeapBase(size_t size) : mSize(size) {
    }

    ResourceHeapBase::~ResourceHeapBase() {
        // Heap must be unmapped before being destroyed.
        ASSERT(mMappedPointer == nullptr);
    }

    void* ResourceHeapBase::GetMappedPointer() const {
        return mMappedPointer;
    }

    size_t ResourceHeapBase::GetSize() const {
        return mSize;
    }

}  // namespace dawn_native