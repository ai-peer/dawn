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

#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/HeapD3D12.h"

namespace dawn_native { namespace d3d12 {

    LRUEntry::LRUEntry(uint64_t size) {
        mSize = size;
    }

    uint64_t LRUEntry::GetSize() const {
        return mSize;
    }

    uint64_t LRUEntry::GetLastSubmittedSerial() const {
        return mLastUsedSerial;
    }

    void LRUEntry::SetLastSubmittedSerial(Serial serial) {
        mLastUsedSerial = serial;
    }

    bool LRUEntry::IsResident() const {
        return next() != nullptr || previous() != nullptr;
    }

    ResidencyManager::ResidencyManager(Device* device)
        : mDevice(device),
          mResidencyManagementEnabled(
              mDevice->IsToggleEnabled(Toggle::UseD3D12ResidencyManagement)) {
    }

    LRUEntry* ResidencyManager::EvictSingleEntry() {
        ASSERT(!mLRUCache.empty());
        LRUEntry* entry = mLRUCache.head()->value();
        entry->RemoveFromList();
        return entry;
    }

    // Any time we need to make something resident in local memory, we must check that we have
    // enough free memory to make the new object resident while also staying within our budget. If
    // there isn't enough memory, we should evict until there is.
    MaybeError ResidencyManager::EnsureCanMakeResident(uint64_t sizeToMakeResident) {
        if (mResidencyManagementEnabled) {
            mDevice->UpdateVideoMemoryInfo();
            const VideoMemoryInfo* videoMemoryInfo = mDevice->GetVideoMemoryInfo();
            uint64_t memoryUsageAfterMakeResident = sizeToMakeResident + videoMemoryInfo->dawnUsage;
            if (memoryUsageAfterMakeResident > videoMemoryInfo->dawnBudget) {
                std::vector<ID3D12Pageable*> resourcesToEvict;

                uint64_t sizeNeeded = memoryUsageAfterMakeResident - videoMemoryInfo->dawnBudget;
                uint64_t sizeEvicted = 0;
                while (sizeEvicted < sizeNeeded) {
                    LRUEntry* entry = EvictSingleEntry();

                    // Before eviction can occur, we must ensure that the GPU has finished using the
                    // heap.
                    if (mDevice->GetCompletedCommandSerial() < entry->GetLastSubmittedSerial()) {
                        DAWN_TRY(mDevice->WaitForSerial(entry->GetLastSubmittedSerial()));
                    }
                    sizeEvicted += entry->GetSize();
                    Heap* heap = CONTAINING_RECORD(entry, Heap, mLRUEntry);
                    resourcesToEvict.push_back(heap->GetD3D12Pageable().Get());
                }

                DAWN_TRY(CheckHRESULT(mDevice->GetD3D12Device()->Evict(resourcesToEvict.size(),
                                                                       resourcesToEvict.data()),
                                      "Evicting resident heaps to free device local memory"));
            }
        }

        return {};
    }

    // When using CreatePlacedResource, we must ensure the target heap is resident in local memory
    // for the function to succeed.
    MaybeError ResidencyManager::EnsureHeapIsResident(Heap* heap) {
        if (mResidencyManagementEnabled) {
            LRUEntry* entry = heap->GetLRUEntry();

            if (heap->GetLRUEntry()->IsResident()) {
                return {};
            } else {
                DAWN_TRY(EnsureCanMakeResident(heap->GetLRUEntry()->GetSize()));
                ID3D12Pageable* pageableMemory = heap->GetD3D12Pageable().Get();
                mDevice->GetD3D12Device()->MakeResident(1, &pageableMemory);
                mLRUCache.Append(entry);
            }
        }

        return {};
    }

    // Given a list of heaps that are pending usage on the next command list execution, this
    // function will estimate memory needed, evict resources until enough space is available, then
    // make resident any heaps scheduled for usage.
    MaybeError ResidencyManager::ProcessResidency(const std::set<Heap*>& heapsPendingUsage) {
        if (mResidencyManagementEnabled) {
            std::vector<ID3D12Pageable*> heapsToMakeResident;
            uint64_t sizeToMakeResident = 0;

            for (Heap* heap : heapsPendingUsage) {
                LRUEntry* entry = heap->GetLRUEntry();

                if (entry->IsResident()) {
                    entry->RemoveFromList();
                } else {
                    heapsToMakeResident.push_back(heap->GetD3D12Pageable().Get());
                    sizeToMakeResident += entry->GetSize();
                }

                mLRUCache.Append(entry);
                entry->SetLastSubmittedSerial(mDevice->GetPendingCommandSerial());
            }

            DAWN_TRY(EnsureCanMakeResident(sizeToMakeResident));

            if (heapsToMakeResident.size() != 0) {
                // Note that MakeResident is a synchronous function and can add a significant
                // overhead to command recording. In the future, it may be possible to decrease this
                // overhead by using MakeResident on a secondary thread, or by instead making use of
                // the EnqueueMakeResident function (which is not availabe on all Windows 10
                // platforms).
                DAWN_TRY(CheckHRESULT(mDevice->GetD3D12Device()->MakeResident(
                                          heapsToMakeResident.size(), heapsToMakeResident.data()),
                                      "Making scheduled-to-be-used resources resident in "
                                      "device local memory"));
            }
        }

        return {};
    }

    // When a new heap is allocated, the heap will be made resident upon creation. We must track
    // when this happens to avoid calling MakeResident a second time.
    void ResidencyManager::TrackResidentAllocation(Heap* heap) {
        if (mResidencyManagementEnabled) {
            mLRUCache.Append(heap->GetLRUEntry());
        }
    }
}}  // namespace dawn_native::d3d12