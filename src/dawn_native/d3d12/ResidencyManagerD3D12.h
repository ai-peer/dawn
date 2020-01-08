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

#ifndef DAWNNATIVE_D3D12_RESIDENCYMANAGERD3D12_H_
#define DAWNNATIVE_D3D12_RESIDENCYMANAGERD3D12_H_

#include "dawn_native/ObjectBase.h"
#include "dawn_native/d3d12/Forward.h"
#include "dawn_native/d3d12/HeapD3D12.h"
#include "dawn_native/d3d12/ResourceAllocatorManagerD3D12.h"
#include "dawn_native/d3d12/ResourceHeapAllocationD3D12.h"
#include "dawn_native/d3d12/d3d12_platform.h"

#include <vector>

namespace dawn_native { namespace d3d12 {

    class ResourceHeapAllocation;

    class LRUEntry {
      public:
        LRUEntry(ID3D12Pageable* memory, uint64_t size);
        ID3D12Pageable* GetD3D12Pageable() const;
        uint64_t GetSize() const;
        bool IsResident() const;
        void SetIsResident(bool isResident);

        LIST_ENTRY mListEntry;

      private:
        uint64_t mSize;
        bool mIsResident;
        ID3D12Pageable* mD3D12Pageable;
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

    class ResidencyManager {
      public:
        ResidencyManager(Device* device);

        void EnsureCanMakeResident(uint64_t allocationSize);
        void EnsureHeapIsResident(Heap* heap);

        void EvictUponDeallocation(Heap* heap);
        void EvictUponDeallocation(ResourceHeapAllocation* resourceHeapAllocation);

        void ProcessResidency(const std::vector<ResourceHeapAllocation*>& mPendingHeaps);

        void TrackAllocation(ResourceHeapAllocation* resourceHeapAllocation);
        void TrackAllocation(Heap* heap);

      private:
        LRUEntry* GetOrCreateLRUEntry(ResourceHeapAllocation* resourceHeapAllocation);
        LRUEntry* GetOrCreateLRUEntry(Heap* heap);

        Device* mDevice;
        bool mResidencyManagementEnabled;
        LRUCache mLRUCache;
        std::vector<ResourceHeapAllocation*> mPendingHeaps;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_RESIDENCYMANAGERD3D12_H_