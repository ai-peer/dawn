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

#include "dawn_native/d3d12/LRUCacheD3D12.h"

#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/HeapD3D12.h"
#include "dawn_native/d3d12/ResourceAllocatorManagerD3D12.h"

namespace dawn_native { namespace d3d12 {
    LRUEntry::LRUEntry(AllocationType allocationType) {
        mAllocationType = allocationType;
        mListEntry.Flink = nullptr;
        mListEntry.Blink = nullptr;
    }

    bool LRUEntry::IsResident() const {
        return mListEntry.Blink != nullptr || mListEntry.Flink != nullptr;
    }

    LIST_ENTRY* LRUEntry::GetListEntry() {
        return &mListEntry;
    }

    LIST_ENTRY* LRUEntry::GetFLink() const {
        return mListEntry.Flink;
    }

    LIST_ENTRY* LRUEntry::GetBLink() const {
        return mListEntry.Blink;
    }

    void LRUEntry::SetFLink(LIST_ENTRY* fLink) {
        mListEntry.Flink = fLink;
    }

    void LRUEntry::SetBLink(LIST_ENTRY* bLink) {
        mListEntry.Blink = bLink;
    }

    AllocationType LRUEntry::GetAllocationType() const {
        return mAllocationType;
    }

    void LRUEntry::SetAllocationType(AllocationType allocationType) {
        mAllocationType = allocationType;
    }

    uint64_t LRUEntry::GetSize() const {
        return mSize;
    }

    void LRUEntry::SetSize(uint64_t size) {
        mSize = size;
    }

    ID3D12Pageable* LRUEntry::GetD3D12Pageable(Device* device) {
        ID3D12Pageable* d3d12Pageable = nullptr;

        switch (mAllocationType) {
            case AllocationType::kDirect: {
                ResourceHeapAllocation* resourceHeapAllocation =
                    CONTAINING_RECORD(this, ResourceHeapAllocation, mLRUEntry);
                d3d12Pageable = resourceHeapAllocation->GetD3D12Resource().Get();
                break;
            }
            case AllocationType::kSubAllocation: {
                ResourceHeapAllocation* resourceHeapAllocation =
                    CONTAINING_RECORD(this, ResourceHeapAllocation, mLRUEntry);
                Heap* heap =
                    device->GetResourceAllocatorManager()->GetResourceHeap(*resourceHeapAllocation);
                d3d12Pageable = heap->GetD3D12Heap().Get();
                break;
            }
            case AllocationType::kHeap: {
                Heap* heap = CONTAINING_RECORD(this, Heap, mLRUEntry);
                d3d12Pageable = heap->GetD3D12Heap().Get();
                break;
            }
            default:
                UNREACHABLE();
        }

        return d3d12Pageable;
    }

    void LRUEntry::Unlink() {
        ASSERT(IsResident() == true);
        LIST_ENTRY* previousEntry = mListEntry.Blink;
        mListEntry.Flink->Blink = previousEntry;
        previousEntry->Flink = mListEntry.Flink;

        mListEntry.Blink = nullptr;
        mListEntry.Flink = nullptr;
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
        entry->SetFLink(&mListHead);
        entry->SetBLink(mListHead.Blink);

        mListHead.Blink->Flink = entry->GetListEntry();
        mListHead.Blink = entry->GetListEntry();
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
        entry->Unlink();
        return entry;
    }

    // Remove a specific node from the LRU cache.
    void LRUCache::Remove(LRUEntry* entry) {
        ASSERT(entry->IsResident() == true);
        if (entry == nullptr) {
            return;
        }
        entry->Unlink();
    }
}}  // namespace dawn_native::d3d12