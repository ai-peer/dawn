// Copyright 2017 The Dawn Authors
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

#include "dawn/native/d3d12/QueueD3D12.h"

#include "dawn/common/Math.h"
#include "dawn/native/CommandValidation.h"
#include "dawn/native/Commands.h"
#include "dawn/native/DynamicUploader.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d12/CommandAllocatorManager.h"
#include "dawn/native/d3d12/CommandBufferD3D12.h"
#include "dawn/native/d3d12/DeviceD3D12.h"
#include "dawn/native/d3d12/UtilsD3D12.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native::d3d12 {

// static
ResultOrError<Ref<Queue>> Queue::Create(Device* device, const QueueDescriptor* descriptor) {
    Ref<Queue> queue = AcquireRef(new Queue(device, descriptor));
    DAWN_TRY(queue->Initialize());
    return queue;
}

Queue::Queue(Device* device, const QueueDescriptor* descriptor) : QueueBase(device, descriptor) {}

Queue::~Queue() {
    ASSERT(!mPendingCommands.IsOpen());
}

MaybeError Queue::Initialize() {
    SetLabelImpl();

    ID3D12Device* d3d12Device = ToBackend(GetDevice())->GetD3D12Device();

    // If PIX is not attached, the QueryInterface fails. Hence, no need to check the return
    // value.
    mCommandQueue.As(&mD3d12SharingContract);

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    DAWN_TRY(CheckHRESULT(d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)),
                          "D3D12 create command queue"));

    DAWN_TRY(CheckHRESULT(d3d12Device->CreateFence(uint64_t(GetLastSubmittedCommandSerial()),
                                                   D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&mFence)),
                          "D3D12 create fence"));

    mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    ASSERT(mFenceEvent != nullptr);

    // TODO(dawn:1413): Consider folding the command allocator manager in this class.
    mCommandAllocatorManager = std::make_unique<CommandAllocatorManager>(this);
    return {};
}

void Queue::Destroy() {
    // Immediately forget about all pending commands for the case where device is lost on its
    // own and WaitForIdleForDestruction isn't called.
    mPendingCommands.Release();

    if (mFenceEvent != nullptr) {
        ::CloseHandle(mFenceEvent);
    }

    mCommandQueue.Reset();
}

ID3D12CommandQueue* Queue::GetCommandQueue() const {
    return mCommandQueue.Get();
}

ID3D12SharingContract* Queue::GetSharingContract() const {
    return mD3d12SharingContract.Get();
}

MaybeError Queue::SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) {
    CommandRecordingContext* commandContext;
    DAWN_TRY_ASSIGN(commandContext, GetPendingCommandContext());

    TRACE_EVENT_BEGIN1(GetDevice()->GetPlatform(), Recording, "CommandBufferD3D12::RecordCommands",
                       "serial", uint64_t(GetDevice()->GetPendingCommandSerial()));
    for (uint32_t i = 0; i < commandCount; ++i) {
        DAWN_TRY(ToBackend(commands[i])->RecordCommands(commandContext));
    }
    TRACE_EVENT_END1(GetDevice()->GetPlatform(), Recording, "CommandBufferD3D12::RecordCommands",
                     "serial", uint64_t(GetDevice()->GetPendingCommandSerial()));

    return SubmitPendingCommands();
}

MaybeError Queue::SubmitPendingCommands() {
    Device* device = ToBackend(GetDevice());
    ASSERT(device->IsLockedByCurrentThreadIfNeeded());

    DAWN_TRY(mCommandAllocatorManager->Tick(GetCompletedCommandSerial()));

    if (!mPendingCommands.IsOpen() || !mPendingCommands.NeedsSubmit()) {
        return {};
    }

    DAWN_TRY(mPendingCommands.ExecuteCommandList(device, mCommandQueue.Get()););
    return NextSerial();
}

MaybeError Queue::NextSerial() {
    IncrementLastSubmittedCommandSerial();

    TRACE_EVENT1(GetDevice()->GetPlatform(), General, "D3D12Device::SignalFence", "serial",
                 uint64_t(GetLastSubmittedCommandSerial()));

    return CheckHRESULT(
        mCommandQueue->Signal(mFence.Get(), uint64_t(GetLastSubmittedCommandSerial())),
        "D3D12 command queue signal fence");
}

MaybeError Queue::WaitForSerial(ExecutionSerial serial) {
    if (GetCompletedCommandSerial() >= serial) {
        return {};
    }
    DAWN_TRY(CheckHRESULT(mFence->SetEventOnCompletion(uint64_t(serial), mFenceEvent),
                          "D3D12 set event on completion"));
    WaitForSingleObject(mFenceEvent, INFINITE);
    DAWN_TRY(CheckPassedSerials());
    return {};
}

bool Queue::HasPendingCommands() const {
    return mPendingCommands.NeedsSubmit();
}

ResultOrError<ExecutionSerial> Queue::CheckAndUpdateCompletedSerials() {
    ExecutionSerial completedSerial = ExecutionSerial(mFence->GetCompletedValue());
    if (DAWN_UNLIKELY(completedSerial == ExecutionSerial(UINT64_MAX))) {
        // GetCompletedValue returns UINT64_MAX if the device was removed.
        // Try to query the failure reason.
        ID3D12Device* d3d12Device = ToBackend(GetDevice())->GetD3D12Device();
        DAWN_TRY(CheckHRESULT(d3d12Device->GetDeviceRemovedReason(),
                              "ID3D12Device::GetDeviceRemovedReason"));
        // Otherwise, return a generic device lost error.
        return DAWN_DEVICE_LOST_ERROR("Device lost");
    }

    if (completedSerial <= GetCompletedCommandSerial()) {
        return ExecutionSerial(0);
    }

    return completedSerial;
}

void Queue::ForceEventualFlushOfCommands() {
    if (mPendingCommands.IsOpen()) {
        mPendingCommands.SetNeedsSubmit();
    }
}

MaybeError Queue::WaitForIdleForDestruction() {
    // Immediately forget about all pending commands
    mPendingCommands.Release();

    DAWN_TRY(NextSerial());
    // Wait for all in-flight commands to finish executing
    DAWN_TRY(WaitForSerial(GetLastSubmittedCommandSerial()));

    return {};
}

ResultOrError<CommandRecordingContext*> Queue::GetPendingCommandContext(SubmitMode submitMode) {
    Device* device = ToBackend(GetDevice());
    ID3D12Device* d3d12Device = device->GetD3D12Device();

    // Callers of GetPendingCommandList do so to record commands. Only reserve a command
    // allocator when it is needed so we don't submit empty command lists
    if (!mPendingCommands.IsOpen()) {
        DAWN_TRY(mPendingCommands.Open(d3d12Device, mCommandAllocatorManager.get()));
    }
    if (submitMode == SubmitMode::Normal) {
        mPendingCommands.SetNeedsSubmit();
    }
    return &mPendingCommands;
    // XXX
}

void Queue::SetLabelImpl() {
    Device* device = ToBackend(GetDevice());
    // TODO(crbug.com/dawn/1344): When we start using multiple queues this needs to be adjusted
    // so it doesn't always change the default queue's label.
    SetDebugName(device, mCommandQueue.Get(), "Dawn_Queue", GetLabel());
}

}  // namespace dawn::native::d3d12
