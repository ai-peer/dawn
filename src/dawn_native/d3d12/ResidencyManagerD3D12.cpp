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

#include "dawn_native/d3d12/ResidencyManagerD3D12.h"
#include "dawn_native/d3d12/DeviceD3D12.h"

namespace dawn_native { namespace d3d12 {
    LRUEntry::LRUEntry(ID3D12Pageable* pageableMemory, uint64_t size) {
        mD3D12Pageable = pageableMemory;
        mIsResident = false;
        mSize = size;
        mListEntry.Blink = nullptr;
        mListEntry.Flink = nullptr;
    }

    uint64_t LRUEntry::GetSize() const {
        return mSize;
    }

    bool LRUEntry::IsResident() const {
        return mIsResident;
    }

    void LRUEntry::SetIsResident(bool isResident) {
        mIsResident = isResident;
    }

    ID3D12Pageable* LRUEntry::GetD3D12Pageable() const {
        return mD3D12Pageable;
    }

    LRUCache::LRUCache() {
        mListHead.Blink = &mListHead;
        mListHead.Flink = &mListHead;
    }

    LRUCache::~LRUCache() {
        while (mListHead.Flink != &mListHead) {
            delete Evict();
        }
    }

    // Insert a node at the end of the LRU (i.e. mListHead->Blink).
    void LRUCache::Insert(LRUEntry* entry) {
        ASSERT(entry->IsResident() == false);
        entry->mListEntry.Flink = &mListHead;
        entry->mListEntry.Blink = mListHead.Blink;

        mListHead.Blink->Flink = &entry->mListEntry;
        mListHead.Blink = &entry->mListEntry;

        entry->SetIsResident(true);
    }

    // Remove and return the least recently used entry from the LRU (i.e. mListHead->Flink).
    LRUEntry* LRUCache::Evict() {
        if (mListHead.Flink == &mListHead) {
            return nullptr;
        }

        // Retrieve the LRUEntry from the LIST_ENTRY in the LRUCache.
        LRUEntry* entry = CONTAINING_RECORD(mListHead.Flink, LRUEntry, mListEntry);

        mListHead.Flink = mListHead.Flink->Flink;
        mListHead.Flink->Blink = &mListHead;

        entry->SetIsResident(false);

        return entry;
    }

    // Remove a specific node from the LRU cache.
    void LRUCache::Remove(LRUEntry* entry) {
        if (entry == nullptr) {
            return;
        }
        LIST_ENTRY* previousEntry = entry->mListEntry.Blink;
        entry->mListEntry.Flink->Blink = previousEntry;
        previousEntry->Flink = entry->mListEntry.Flink;

        entry->SetIsResident(false);
    }

    ResidencyManager::ResidencyManager(Device* device) : mDevice(device) {
    }

    // Any time we need to make something resident in local memory, we must check that we have
    // enough free memory to make the new object resident while also staying within our budget. If
    // there isn't enough memory, we should evict until there is.
    void ResidencyManager::EnsureCanMakeResident(uint64_t sizeToMakeResident) {
        const VideoMemoryInfo* videoMemoryInfo = mDevice->GetVideoMemoryInfo();
        uint64_t videoMemoryCap = videoMemoryInfo->dawnBudget * 0.8;
        uint64_t memoryUsageAfterMakeResident = sizeToMakeResident + videoMemoryInfo->dawnUsage;
        if (memoryUsageAfterMakeResident > videoMemoryCap) {
            std::vector<ID3D12Pageable*> resourcesToEvict;

            uint64_t sizeNeeded = memoryUsageAfterMakeResident - videoMemoryCap;
            uint64_t sizeEvicted = 0;
            while (sizeEvicted < sizeNeeded) {
                LRUEntry* evictedEntry = mLRUCache.Evict();
                if (evictedEntry == nullptr) {
                    UNREACHABLE();
                }

                sizeEvicted += evictedEntry->GetSize();
                resourcesToEvict.push_back(evictedEntry->GetD3D12Pageable());
            }
            SUCCEEDED(
                mDevice->GetD3D12Device()->Evict(resourcesToEvict.size(), resourcesToEvict.data()));
        }
    }

    // When using CreatePlacedResource, we must ensure the target heap is resident in local memory
    // for the function to succeed.
    void ResidencyManager::EnsureHeapIsResident(Heap* heap) {
        LRUEntry* entry = heap->GetResidencyLRUEntry();
        ASSERT(entry != nullptr);

        if (entry->IsResident()) {
            return;
        } else {
            EnsureCanMakeResident(entry->GetSize());
            ID3D12Pageable* pageableMemory = entry->GetD3D12Pageable();
            mDevice->GetD3D12Device()->MakeResident(1, &pageableMemory);
            mLRUCache.Insert(entry);
        }
    }

    void ResidencyManager::EvictUponDeallocation(Heap* heap) {
        LRUEntry* entry = heap->GetResidencyLRUEntry();
        mLRUCache.Remove(entry);
        delete entry;
    }

    void ResidencyManager::EvictUponDeallocation(ResourceHeapAllocation* resourceHeapAllocation) {
        ASSERT(resourceHeapAllocation->GetInfo().mMethod == AllocationMethod::kDirect);
        LRUEntry* entry = resourceHeapAllocation->GetResidencyLRUEntry();
        mLRUCache.Remove(entry);
        delete entry;
    }

    LRUEntry* ResidencyManager::GetOrCreateLRUEntry(Heap* heap) {
        LRUEntry* entry = heap->GetResidencyLRUEntry();

        if (entry == nullptr) {
            entry = new LRUEntry(heap->GetD3D12Heap().Get(),
                                 heap->GetD3D12Heap()->GetDesc().SizeInBytes);
            heap->SetResidencyLRUEntry(entry);
        }

        return entry;
    }

    LRUEntry* ResidencyManager::GetOrCreateLRUEntry(ResourceHeapAllocation* heapAllocation) {
        LRUEntry* entry = nullptr;

        if (heapAllocation->GetInfo().mMethod == AllocationMethod::kDirect) {
            entry = heapAllocation->GetResidencyLRUEntry();

            if (entry == nullptr) {
                D3D12_RESOURCE_DESC desc = heapAllocation->GetD3D12Resource()->GetDesc();
                entry = new LRUEntry(
                    heapAllocation->GetD3D12Resource().Get(),
                    mDevice->GetD3D12Device()->GetResourceAllocationInfo(0, 1, &desc).SizeInBytes);
                heapAllocation->SetResidencyLRUEntry(entry);
            }
        } else if (heapAllocation->GetInfo().mMethod == AllocationMethod::kSubAllocated) {
            Heap* heap = mDevice->GetResourceAllocatorManager()->GetResourceHeap(*heapAllocation);
            entry = GetOrCreateLRUEntry(heap);
        } else {
            UNREACHABLE();
        }

        return entry;
    }

    // Used in conjunction with TrackUsage(), this function will estimate memory, evict, then make
    // resident any resources scheduled for usage.
    void ResidencyManager::ProcessResidency(
        const std::vector<ResourceHeapAllocation*>& mPendingHeaps) {
        std::vector<ID3D12Pageable*> heapsToMakeResident;
        uint64_t makeResidentSize = 0;
        for (uint32_t i = 0; i < mPendingHeaps.size(); i++) {
            LRUEntry* entry = GetOrCreateLRUEntry(mPendingHeaps[i]);

            if (entry->IsResident()) {
                mLRUCache.Remove(entry);
            } else {
                heapsToMakeResident.push_back(entry->GetD3D12Pageable());
                makeResidentSize += entry->GetSize();
            }

            mLRUCache.Insert(entry);
        }

        EnsureCanMakeResident(makeResidentSize);

        if (heapsToMakeResident.size() != 0) {
            SUCCEEDED(mDevice->GetD3D12Device()->MakeResident(heapsToMakeResident.size(),
                                                              heapsToMakeResident.data()));
        }
    }

    // When using CreateCommittedResource, the underlying heap will implicitly be made resident
    // upon creation. We must track when this happens to avoid calling MakeResident a second time.
    void ResidencyManager::TrackAllocation(ResourceHeapAllocation* resourceHeapAllocation) {
        LRUEntry* entry = GetOrCreateLRUEntry(resourceHeapAllocation);
        mLRUCache.Insert(entry);
    }

    // When using CreateHeap, the heap will be made resident upon creation. We must track when this
    // happens to avoid calling MakeResident a second time.
    void ResidencyManager::TrackAllocation(Heap* heap) {
        LRUEntry* entry = GetOrCreateLRUEntry(heap);
        mLRUCache.Insert(entry);
    }
}}  // namespace dawn_native::d3d12