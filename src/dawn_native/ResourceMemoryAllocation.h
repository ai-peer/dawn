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

#ifndef DAWNNATIVE_RESOURCEMEMORYALLOCATION_H_
#define DAWNNATIVE_RESOURCEMEMORYALLOCATION_H_

#include <cstddef>

namespace dawn_native {

    class ResourceHeapBase;

    // Handle into a heap pool.
    class ResourceMemoryAllocation {
      public:
        ResourceMemoryAllocation() = default;
        ResourceMemoryAllocation(size_t offset,
                                 ResourceHeapBase* resourceHeap = nullptr,
                                 bool isDirect = false);
        ~ResourceMemoryAllocation() = default;

        ResourceHeapBase* GetResourceHeap() const;
        size_t GetOffset() const;
        bool IsDirect() const;

      private:
        bool mIsDirect;
        size_t mOffset;
        ResourceHeapBase* mResourceHeap = nullptr;
    };
}  // namespace dawn_native

#endif  // DAWNNATIVE_RESOURCEMEMORYALLOCATION_H_