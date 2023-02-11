// Copyright 2019 The Dawn Authors
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

#include "dawn/native/d3d11/CommandRecordingContextD3D11.h"

#include <profileapi.h>
#include <sysinfoapi.h>

#include <string>
#include <utility>

// #include "dawn/native/d3d11/CommandAllocatorManager.h"
#include "dawn/native/d3d11/D3D11Error.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
// #include "dawn/native/d3d11/HeapD3D11.h"
// #include "dawn/native/d3d11/ResidencyManagerD3D11.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native::d3d11 {

void CommandRecordingContext::AddToSharedTextureList(Texture* texture) {
    ASSERT(IsOpen());
    mSharedTextures.insert(texture);
}

MaybeError CommandRecordingContext::Open(ID3D11Device* d3d11Device) {
    ASSERT(!IsOpen());
    ASSERT(d3d11Device);

    ComPtr<ID3D11DeviceContext> d3d11DeviceContext;
    d3d11Device->GetImmediateContext(&d3d11DeviceContext);

    DAWN_TRY(CheckHRESULT(d3d11DeviceContext.As(&mD3D11DeviceContext1),
                          "D3D11 querying immediate context for ID3D11DeviceContext1 interface"));
    mD3D11Device = d3d11Device;
    mIsOpen = true;
    mNeedsSubmit = false;

    return {};
}

MaybeError CommandRecordingContext::ExecuteCommandList(Device* device) {
    if (IsOpen()) {
        //     // Shared textures must be transitioned to common state after the last usage in order
        //     // for them to be used by other APIs like D3D11. We ensure this by transitioning to
        //     the
        //     // common state right before command list submission. TransitionUsageNow itself
        //     ensures
        //     // no unnecessary transitions happen if the resources is already in the common state.
        //     for (Texture* texture : mSharedTextures) {
        //         DAWN_TRY(texture->SynchronizeImportedTextureBeforeUse());
        //         texture->TrackAllUsageAndTransitionNow(this, D3D11_RESOURCE_STATE_COMMON);
        //     }

        //     MaybeError error =
        //         CheckHRESULT(mD3d12CommandList->Close(), "D3D11 closing pending command list");
        //     if (error.IsError()) {
        //         Release();
        //         DAWN_TRY(std::move(error));
        //     }
        //     DAWN_TRY(device->GetResidencyManager()->EnsureHeapsAreResident(mHeapsPendingUsage.data(),
        //                                                                    mHeapsPendingUsage.size()));

        //     if (device->IsToggleEnabled(Toggle::RecordDetailedTimingInTraceEvents)) {
        //         uint64_t gpuTimestamp;
        //         uint64_t cpuTimestamp;
        //         FILETIME fileTimeNonPrecise;
        //         SYSTEMTIME systemTimeNonPrecise;

        //         // Both supported since Windows 2000, have a accuracy of 1ms
        //         GetSystemTimeAsFileTime(&fileTimeNonPrecise);
        //         GetSystemTime(&systemTimeNonPrecise);
        //         // Query CPU and GPU timestamps at almost the same time
        //         device->GetCommandQueue()->GetClockCalibration(&gpuTimestamp, &cpuTimestamp);

        //         uint64_t gpuFrequency;
        //         uint64_t cpuFrequency;
        //         LARGE_INTEGER cpuFrequencyLargeInteger;
        //         device->GetCommandQueue()->GetTimestampFrequency(&gpuFrequency);
        //         QueryPerformanceFrequency(&cpuFrequencyLargeInteger);  // Supported since Windows
        //         2000 cpuFrequency = cpuFrequencyLargeInteger.QuadPart;

        //         std::string timingInfo = absl::StrFormat(
        //             "UTC Time: %u/%u/%u %02u:%02u:%02u.%03u, File Time: %u, CPU "
        //             "Timestamp: %u, GPU Timestamp: %u, CPU Tick Frequency: %u, GPU Tick
        //             Frequency: "
        //             "%u",
        //             systemTimeNonPrecise.wYear, systemTimeNonPrecise.wMonth,
        //             systemTimeNonPrecise.wDay, systemTimeNonPrecise.wHour,
        //             systemTimeNonPrecise.wMinute, systemTimeNonPrecise.wSecond,
        //             systemTimeNonPrecise.wMilliseconds,
        //             (static_cast<uint64_t>(fileTimeNonPrecise.dwHighDateTime) << 32) +
        //                 fileTimeNonPrecise.dwLowDateTime,
        //             cpuTimestamp, gpuTimestamp, cpuFrequency, gpuFrequency);

        //         TRACE_EVENT_INSTANT1(
        //             device->GetPlatform(), General,
        //             "d3d11::CommandRecordingContext::ExecuteCommandList Detailed Timing",
        //             "Timing", timingInfo.c_str());
        //     }

        //     ID3D11CommandList* d3d11CommandList = GetCommandList();
        //     device->GetCommandQueue()->ExecuteCommandLists(1, &d3d11CommandList);

        //     for (Texture* texture : mSharedTextures) {
        //         DAWN_TRY(texture->SynchronizeImportedTextureAfterUse());
        //     }

        Release();
    }
    return {};
}

void CommandRecordingContext::TrackHeapUsage(Heap* heap, ExecutionSerial serial) {
    // // Before tracking the heap, check the last serial it was recorded on to ensure we aren't
    // // tracking it more than once.
    // if (heap->GetLastUsage() < serial) {
    //     heap->SetLastUsage(serial);
    //     mHeapsPendingUsage.push_back(heap);
    // }
}

ID3D11Device* CommandRecordingContext::GetD3D11Device() const {
    return mD3D11Device.Get();
}

ID3D11DeviceContext* CommandRecordingContext::GetD3D11DeviceContext() const {
    return mD3D11DeviceContext1.Get();
}

ID3D11DeviceContext1* CommandRecordingContext::GetD3D11DeviceContext1() const {
    return mD3D11DeviceContext1.Get();
}

void CommandRecordingContext::Release() {
    mIsOpen = false;
    mNeedsSubmit = false;
    mSharedTextures.clear();
    mHeapsPendingUsage.clear();
    mD3D11DeviceContext1 = nullptr;
    mD3D11Device = nullptr;
}

bool CommandRecordingContext::IsOpen() const {
    return mIsOpen;
}

bool CommandRecordingContext::NeedsSubmit() const {
    return mNeedsSubmit;
}

void CommandRecordingContext::SetNeedsSubmit() {
    mNeedsSubmit = true;
}

// void CommandRecordingContext::AddToTempBuffers(Ref<Buffer> tempBuffer) {
//     mTempBuffers.emplace_back(tempBuffer);
// }

}  // namespace dawn::native::d3d11
