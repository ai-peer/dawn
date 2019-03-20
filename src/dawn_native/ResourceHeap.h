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

#ifndef DAWNNATIVE_RESOURCEHEAP_H_
#define DAWNNATIVE_RESOURCEHEAP_H_

#include "dawn_native/Error.h"

namespace dawn_native {

    // Wrapper for a resource backed by a heap.
    class ResourceHeapBase {
      public:
        ResourceHeapBase(size_t size);
        virtual ~ResourceHeapBase();

        virtual ResultOrError<void*> Map() = 0;
        virtual void Unmap() = 0;

        size_t GetSize() const;
        void* GetMappedPointer() const;

      protected:
        void* mMappedPointer = nullptr;
        size_t mSize = 0;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_RESOURCEHEAP_H_