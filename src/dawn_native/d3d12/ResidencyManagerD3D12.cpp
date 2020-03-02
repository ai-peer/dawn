#include "ResidencyManagerD3D12.h"
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

#include "dawn_native/d3d12/AdapterD3D12.h"
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/Forward.h"
#include "dawn_native/d3d12/HeapD3D12.h"

#include "dawn_native/d3d12/d3d12_platform.h"

namespace dawn_native { namespace d3d12 {

    ResidencyManager::ResidencyManager(Device* device)
        : mDevice(device),
          mResidencyManagementEnabled(
              device->IsToggleEnabled(Toggle::UseD3D12ResidencyManagement)) {
        UpdateVideoMemoryInfo();
    }

    // Allows an application component external to Dawn to cap Dawn's residency budget to prevent
    // competition for device local memory. Returns the amount of memory reserved, which may be less
    // that the requested reservation when under pressure.
    uint64_t ResidencyManager::SetExternalMemoryReservation(uint64_t requestedReservationSize) {
        mVideoMemoryInfo.externalRequest = requestedReservationSize;
        UpdateVideoMemoryInfo();
        return mVideoMemoryInfo.externalReservation;
    }

    void ResidencyManager::UpdateVideoMemoryInfo() {
        if (!mResidencyManagementEnabled) {
            return;
        }

        DXGI_QUERY_VIDEO_MEMORY_INFO queryVideoMemoryInfo;
        ToBackend(mDevice->GetAdapter())
            ->GetHardwareAdapter()
            ->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &queryVideoMemoryInfo);

        // The video memory budget provided by QueryVideoMemoryInfo is defined by the operating
        // system, and may be lower than expected in certain scenarios. Under memory pressure, we
        // cap the external reservation to half the available budget, which prevents the external
        // component from consuming a disproportionate share of memory and ensures that Dawn can
        // continue to make forward progress.
        mVideoMemoryInfo.externalReservation =
            std::max(queryVideoMemoryInfo.Budget / 2, mVideoMemoryInfo.externalReservation);

        // We cap Dawn's budget to 95% of the provided budget. Leaving some budget unused
        // decreases fluctuations in the operating-system-defined budget, which improves stability
        // for both Dawn and other applications on the system. Note this value is arbitrarily chosen
        // and subject to experimentation.
        static constexpr float kBudgetCap = 0.95;
        mVideoMemoryInfo.dawnBudget =
            (queryVideoMemoryInfo.Budget - mVideoMemoryInfo.externalReservation) * kBudgetCap;
        mVideoMemoryInfo.dawnUsage =
            queryVideoMemoryInfo.CurrentUsage - mVideoMemoryInfo.externalReservation;
    }

    Heap* ResidencyManager::EvictSingleEntry(Serial pendingCommandSerial) {
        ASSERT(!mLRUCache.empty());
        Heap* heap = mLRUCache.head()->value();
        if (pendingCommandSerial == heap->GetLastSubmissionSerial()) {
            return nullptr;
        }
        heap->RemoveFromList();

        return heap;
    }

    // Any time we need to make something resident in local memory, we must check that we have
    // enough free memory to make the new object resident while also staying within our budget.
    // If there isn't enough memory, we should evict until there is.
    MaybeError ResidencyManager::EnsureCanMakeResident(uint64_t sizeToMakeResident) {
        if (!mResidencyManagementEnabled) {
            return {};
        }

        UpdateVideoMemoryInfo();

        uint64_t memoryUsageAfterMakeResident = sizeToMakeResident + mVideoMemoryInfo.dawnUsage;

        if (memoryUsageAfterMakeResident < mVideoMemoryInfo.dawnBudget) {
            return {};
        }

        std::vector<ID3D12Pageable*> resourcesToEvict;

        uint64_t sizeNeeded = memoryUsageAfterMakeResident - mVideoMemoryInfo.dawnBudget;
        uint64_t sizeEvicted = 0;
        while (sizeEvicted < sizeNeeded) {
            Heap* heap = EvictSingleEntry(mDevice->GetPendingCommandSerial());

            if (heap == nullptr) {
                break;
            }

            // Before eviction can occur, we must ensure that the GPU has finished using the
            // heap.
            if (mDevice->GetCompletedCommandSerial() < heap->GetLastSubmissionSerial()) {
                DAWN_TRY(mDevice->WaitForSerial(heap->GetLastSubmissionSerial()));
            }

            if (heap->IsResidencyLocked()) {
                continue;
            }

            sizeEvicted += heap->GetSize();
            resourcesToEvict.push_back(heap->GetD3D12Pageable().Get());
        }

        DAWN_TRY(CheckHRESULT(
            mDevice->GetD3D12Device()->Evict(resourcesToEvict.size(), resourcesToEvict.data()),
            "Evicting resident heaps to free device local memory"));

        return {};
    }

    // Given a list of heaps that are pending usage, this function will estimate memory needed,
    // evict resources until enough space is available, then make resident any heaps scheduled for
    // usage.
    MaybeError ResidencyManager::EnsureHeapsAreResident(Heap** heaps,
                                                        size_t heapCount,
                                                        bool keepResidentUntilNextSerial) {
        if (!mResidencyManagementEnabled) {
            return {};
        }

        std::vector<ID3D12Pageable*> heapsToMakeResident;
        uint64_t sizeToMakeResident = 0;

        for (size_t i = 0; i < heapCount; i++) {
            if (heaps[i]->IsResident()) {
                heaps[i]->RemoveFromList();
            } else {
                heapsToMakeResident.push_back(heaps[i]->GetD3D12Pageable().Get());
                sizeToMakeResident += heaps[i]->GetSize();
            }

            mLRUCache.Append(heaps[i]);
            if (keepResidentUntilNextSerial) {
                heaps[i]->SetLastSubmissionSerial(mDevice->GetPendingCommandSerial());
            }
        }

        if (heapsToMakeResident.size() != 0) {
            DAWN_TRY(EnsureCanMakeResident(sizeToMakeResident));

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

        return {};
    }

    // When a new heap is allocated, the heap will be made resident upon creation. We must track
    // when this happens to avoid calling MakeResident a second time.
    void ResidencyManager::TrackResidentAllocation(Heap* heap) {
        if (!mResidencyManagementEnabled) {
            return;
        }

        mLRUCache.Append(heap);
    }
}}  // namespace dawn_native::d3d12