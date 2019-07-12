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

    ResourceHeapBase::~ResourceHeapBase() {
        // Heap was not unmapped before being destroyed.
        ASSERT(mMappedPointer == nullptr);
    }

    ResultOrError<void*> ResourceHeapBase::Map() {
        DAWN_TRY(MapImpl());
        return mMappedPointer;
    }

    void ResourceHeapBase::Unmap() {
        UnmapImpl();
        mMappedPointer = nullptr;
    }

}  // namespace dawn_native