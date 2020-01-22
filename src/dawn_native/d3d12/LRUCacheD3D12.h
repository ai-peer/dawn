// Copyright 2020 The Dawn Authors
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

#ifndef DAWNNATIVE_D3D12_LRUCACHED3D12_H_
#define DAWNNATIVE_D3D12_LRUCACHED3D12_H_

#include "dawn_native/d3d12/Forward.h"
#include "dawn_native/ResourceMemoryAllocation.h"

#include "dawn_native/d3d12/d3d12_platform.h"

namespace dawn_native { namespace d3d12 {

    class ResourceHeapAllocation;
    class Heap;
    class LRUCache;

    enum AllocationType {
        kDirect,
        kExternal,
        kSubAllocation,
        kHeap,
        kInvalid
    };

    class LRUEntry {
      public:
        LRUEntry() = default;
        LRUEntry(AllocationType allocationType);
        
        bool IsResident() const;
        
        AllocationType GetAllocationType() const;
        void SetAllocationType(AllocationType allocationType);

        LIST_ENTRY* GetFLink() const;
        LIST_ENTRY* GetBLink() const;
        LIST_ENTRY* GetListEntry();
        void SetFLink(LIST_ENTRY* FLink);
        void SetBLink(LIST_ENTRY* BLink);

        uint64_t GetSize() const;
        void SetSize(uint64_t);

        ID3D12Pageable* GetD3D12Pageable(Device* device);

        void Unlink();

      private:
        AllocationType mAllocationType = AllocationType::kInvalid;
        LIST_ENTRY mListEntry;
        uint64_t mSize = 0;

        friend LRUCache;
    };

     class LRUCache {
      public:
        LRUCache();
        ~LRUCache();
        LRUEntry* Evict();
        void Insert(LRUEntry* node);
        void Remove(LRUEntry* node);

      private:
        LIST_ENTRY mListHead;
    };
    
}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_LRUCACHED3D12_H_