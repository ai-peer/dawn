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
#include "dawn_native/d3d12/ResourceAllocatorManagerD3D12.h"
#include "dawn_native/d3d12/LRUCacheD3D12.h"

namespace dawn_native { namespace d3d12 {

    ResidencyManager::ResidencyManager(Device* device)
        : mDevice(device),
          mResidencyManagementEnabled(
              mDevice->IsToggleEnabled(Toggle::UseD3D12ResidencyManagement)) {
    }

    // Any time we need to make something resident in local memory, we must check that we have
    // enough free memory to make the new object resident while also staying within our budget. If
    // there isn't enough memory, we should evict until there is.
    MaybeError ResidencyManager::EnsureCanMakeResident(uint64_t sizeToMakeResident) {
        if (mResidencyManagementEnabled) {
            const VideoMemoryInfo* videoMemoryInfo = mDevice->GetVideoMemoryInfo();
            uint64_t memoryUsageAfterMakeResident = sizeToMakeResident + videoMemoryInfo->dawnUsage;
            if (memoryUsageAfterMakeResident > videoMemoryInfo->dawnBudget) {
                std::vector<ID3D12Pageable*> resourcesToEvict;

                uint64_t sizeNeeded = memoryUsageAfterMakeResident - videoMemoryInfo->dawnBudget;
                uint64_t sizeEvicted = 0;
                while (sizeEvicted < sizeNeeded) {
                    LRUEntry* evictedEntry = mLRUCache.Evict();
                    if (evictedEntry == nullptr) {
                        UNREACHABLE();
                    }

                    sizeEvicted += evictedEntry->GetSize();
                    resourcesToEvict.push_back(evictedEntry->GetD3D12Pageable(mDevice));
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
            ASSERT(entry != nullptr);

            if (entry->IsResident()) {
                return {};
            } else {
                DAWN_TRY(EnsureCanMakeResident(entry->GetSize()));
                ID3D12Pageable* pageableMemory = entry->GetD3D12Pageable(mDevice);
                mDevice->GetD3D12Device()->MakeResident(1, &pageableMemory);
                mLRUCache.Insert(entry);
            }
        }

        return {};
    }

    LRUEntry* ResidencyManager::GetLRUEntry(ResourceHeapAllocation* resourceHeapAllocation) {
        LRUEntry* entry = nullptr;

        if (resourceHeapAllocation->GetInfo().mMethod == AllocationMethod::kDirect) {
            entry = resourceHeapAllocation->GetLRUEntry();
        } else if (resourceHeapAllocation->GetInfo().mMethod == AllocationMethod::kSubAllocated) {
            Heap* heap = mDevice->GetResourceAllocatorManager()->GetResourceHeap(*resourceHeapAllocation);
            entry = heap->GetLRUEntry();
        } else {
            UNREACHABLE();
        }

        return entry;
    }

    // Given a list of heaps that are pending usage on the next command list execution, this
    // function will estimate memory needed, evict resources until enough space is available, then
    // make resident any heaps scheduled for usage.
    MaybeError ResidencyManager::ProcessResidency(
        const std::vector<ResourceHeapAllocation*>& pendingResidentList) {
        if (mResidencyManagementEnabled) {
            std::vector<ID3D12Pageable*> heapsToMakeResident;
            uint64_t sizeToMakeResident = 0;
            for (uint32_t i = 0; i < pendingResidentList.size(); i++) {
                LRUEntry* entry = GetLRUEntry(pendingResidentList[i]);

                if (entry->IsResident()) {
                    mLRUCache.Remove(entry);
                } else {
                    ID3D12Pageable* d3d12Pageable = entry->GetD3D12Pageable(mDevice);
                    if (d3d12Pageable != nullptr) {
                        heapsToMakeResident.push_back(entry->GetD3D12Pageable(mDevice));
                    }
                    sizeToMakeResident += entry->GetSize();
                }

                mLRUCache.Insert(entry);
            }

            DAWN_TRY(EnsureCanMakeResident(sizeToMakeResident));

            if (heapsToMakeResident.size() != 0) {
                // Note that MakeResident is a synchronous functiona and can add a significant
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

    // When using CreateCommittedResource, the underlying heap will implicitly be made resident
    // upon creation. We must track when this happens to avoid calling MakeResident a second time.
    void ResidencyManager::TrackResidentAllocation(ResourceHeapAllocation* resourceHeapAllocation) {
        if (mResidencyManagementEnabled) {
            LRUEntry* entry = resourceHeapAllocation->GetLRUEntry();
            AllocationMethod allocationMethod = resourceHeapAllocation->GetInfo().mMethod;
            if (allocationMethod == AllocationMethod::kSubAllocated) {
                entry->SetAllocationType(AllocationType::kSubAllocation);
                return;
            } else if (allocationMethod == AllocationMethod::kDirect) {
                entry->SetAllocationType(AllocationType::kDirect);
                D3D12_RESOURCE_DESC desc = resourceHeapAllocation->GetD3D12Resource()->GetDesc();
                entry->SetSize(mDevice->GetD3D12Device()->GetResourceAllocationInfo(0, 1, &desc).SizeInBytes);
                mLRUCache.Insert(entry);
            }
        }
    }

    // When using CreateHeap, the heap will be made resident upon creation. We must track when this
    // happens to avoid calling MakeResident a second time.
    void ResidencyManager::TrackResidentAllocation(Heap* heap) {
        if (mResidencyManagementEnabled) {
            LRUEntry* entry = heap->GetLRUEntry();
            entry->SetAllocationType(AllocationType::kHeap);
            mLRUCache.Insert(entry);
            entry->SetSize(heap->GetD3D12Heap()->GetDesc().SizeInBytes);
        }
    }
}}  // namespace dawn_native::d3d12