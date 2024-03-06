// Copyright 2020 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "dawn/native/CreatePipelineAsyncTask.h"

#include <utility>

#include "dawn/native/AsyncTask.h"
#include "dawn/native/ComputePipeline.h"
#include "dawn/native/Device.h"
#include "dawn/native/RenderPipeline.h"
#include "dawn/native/utils/WGPUHelpers.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/metrics/HistogramMacros.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native {

CreateComputePipelineAsyncTask::CreateComputePipelineAsyncTask(
    Ref<ComputePipelineBase> nonInitializedComputePipeline)
    : mPipeline(std::move(nonInitializedComputePipeline)),
      mScopedUseShaderPrograms(mPipeline->UseShaderPrograms()) {}

CreateComputePipelineAsyncTask::~CreateComputePipelineAsyncTask() = default;

void CreateComputePipelineAsyncTask::Run(CreateComputePipelineAsyncEvent* event) {
    const char* eventLabel = utils::GetLabelForTrace(mPipeline->GetLabel().c_str());
    DeviceBase* device = mPipeline->GetDevice();
    TRACE_EVENT_FLOW_END1(device->GetPlatform(), General,
                          "CreateComputePipelineAsyncTask::RunAsync", this, "label", eventLabel);
    TRACE_EVENT1(device->GetPlatform(), General, "CreateComputePipelineAsyncTask::Run", "label",
                 eventLabel);

    MaybeError maybeError;
    {
        SCOPED_DAWN_HISTOGRAM_TIMER_MICROS(device->GetPlatform(), "CreateComputePipelineUS");
        maybeError = mPipeline->Initialize(std::move(mScopedUseShaderPrograms));
    }
    DAWN_HISTOGRAM_BOOLEAN(device->GetPlatform(), "CreateComputePipelineSuccess",
                           maybeError.IsSuccess());
    if (maybeError.IsError()) {
        event->mError = maybeError.AcquireError();
    }

    std::get<Ref<SystemEvent>>(event->GetCompletionData())->Signal();
}

void CreateComputePipelineAsyncTask::RunAsync(DeviceBase* device,
                                              CreateComputePipelineAsyncEvent* event) {
    const char* eventLabel = utils::GetLabelForTrace(event->mTask->mPipeline->GetLabel().c_str());
    TRACE_EVENT_FLOW_BEGIN1(device->GetPlatform(), General,
                            "CreateComputePipelineAsyncTask::RunAsync", event->mTask.get(), "label",
                            eventLabel);

    auto asyncTask = [event = event] { event->mTask->Run(event); };
    device->GetAsyncTaskManager()->PostTask(std::move(asyncTask));
}

CreateComputePipelineAsyncEvent::CreateComputePipelineAsyncEvent(
    DeviceBase* device,
    const CreateComputePipelineAsyncCallbackInfo& callbackInfo,
    Ref<ComputePipelineBase> pipeline,
    Ref<SystemEvent> systemEvent,
    std::unique_ptr<CreateComputePipelineAsyncTask> task)
    : TrackedEvent(callbackInfo.mode, std::move(systemEvent)),
      mCallback(callbackInfo.callback),
      mUserdata(callbackInfo.userdata),
      mPipeline(std::move(pipeline)),
      mTask(std::move(task)) {}

CreateComputePipelineAsyncEvent::CreateComputePipelineAsyncEvent(
    DeviceBase* device,
    const CreateComputePipelineAsyncCallbackInfo& callbackInfo,
    Ref<ComputePipelineBase> pipeline)
    : TrackedEvent(callbackInfo.mode, TrackedEvent::Completed{}),
      mCallback(callbackInfo.callback),
      mUserdata(callbackInfo.userdata),
      mPipeline(std::move(pipeline)) {}

CreateComputePipelineAsyncEvent::CreateComputePipelineAsyncEvent(
    DeviceBase* device,
    const CreateComputePipelineAsyncCallbackInfo& callbackInfo,
    std::unique_ptr<ErrorData> error,
    const char* label)
    : TrackedEvent(callbackInfo.mode, TrackedEvent::Completed{}),
      mCallback(callbackInfo.callback),
      mUserdata(callbackInfo.userdata),
      mPipeline(ComputePipelineBase::MakeError(device, label)),
      mError(std::move(error)) {}

CreateComputePipelineAsyncEvent::~CreateComputePipelineAsyncEvent() {
    EnsureComplete(EventCompletionType::Shutdown);
}

void CreateComputePipelineAsyncEvent::Complete(EventCompletionType completionType) {
    DeviceBase* device = mPipeline->GetDevice();
    if (mError != nullptr) {
        wgpu::CreatePipelineAsyncStatus status;
        switch (mError->GetType()) {
            case InternalErrorType::Validation:
                status = wgpu::CreatePipelineAsyncStatus::ValidationError;
                break;
            case InternalErrorType::DeviceLost:
                status = wgpu::CreatePipelineAsyncStatus::DeviceLost;
                break;
            default:
                status = wgpu::CreatePipelineAsyncStatus::InternalError;
                break;
        }

        if (device->IsLost() || status == wgpu::CreatePipelineAsyncStatus::DeviceLost) {
            // Invalid async creation should "succeed" if the device is already lost.
            mCallback(ToAPI(wgpu::CreatePipelineAsyncStatus::Success),
                      ToAPI(ReturnToAPI(std::move(mPipeline))), "Device lost", mUserdata);
        } else {
            mCallback(ToAPI(status), nullptr, mError->GetFormattedMessage().c_str(), mUserdata);
        }
        return;
    }

    {
        auto deviceLock(device->GetScopedLock());
        if (device->GetState() == DeviceBase::State::Alive) {
            mPipeline = device->AddOrGetCachedComputePipeline(std::move(mPipeline));
        }
    }
    mCallback(ToAPI(wgpu::CreatePipelineAsyncStatus::Success),
              ToAPI(ReturnToAPI(std::move(mPipeline))), "", mUserdata);
}

CreateRenderPipelineAsyncTask::CreateRenderPipelineAsyncTask(
    Ref<RenderPipelineBase> nonInitializedRenderPipeline,
    WGPUCreateRenderPipelineAsyncCallback callback,
    void* userdata)
    : mRenderPipeline(std::move(nonInitializedRenderPipeline)),
      mCallback(callback),
      mUserdata(userdata),
      mScopedUseShaderPrograms(mRenderPipeline->UseShaderPrograms()) {
    DAWN_ASSERT(mRenderPipeline != nullptr);
}

CreateRenderPipelineAsyncTask::~CreateRenderPipelineAsyncTask() = default;

void CreateRenderPipelineAsyncTask::Run() {
    const char* eventLabel = utils::GetLabelForTrace(mRenderPipeline->GetLabel().c_str());

    DeviceBase* device = mRenderPipeline->GetDevice();
    TRACE_EVENT_FLOW_END1(device->GetPlatform(), General, "CreateRenderPipelineAsyncTask::RunAsync",
                          this, "label", eventLabel);
    TRACE_EVENT1(device->GetPlatform(), General, "CreateRenderPipelineAsyncTask::Run", "label",
                 eventLabel);

    MaybeError maybeError;
    {
        SCOPED_DAWN_HISTOGRAM_TIMER_MICROS(device->GetPlatform(), "CreateRenderPipelineUS");
        maybeError = mRenderPipeline->Initialize(std::move(mScopedUseShaderPrograms));
    }
    DAWN_HISTOGRAM_BOOLEAN(device->GetPlatform(), "CreateRenderPipelineSuccess",
                           maybeError.IsSuccess());
    if (maybeError.IsError()) {
        device->AddRenderPipelineAsyncCallbackTask(
            maybeError.AcquireError(), mRenderPipeline->GetLabel().c_str(), mCallback, mUserdata);
    } else {
        device->AddRenderPipelineAsyncCallbackTask(mRenderPipeline, mCallback, mUserdata);
    }
}

void CreateRenderPipelineAsyncTask::RunAsync(std::unique_ptr<CreateRenderPipelineAsyncTask> task) {
    DeviceBase* device = task->mRenderPipeline->GetDevice();

    const char* eventLabel = utils::GetLabelForTrace(task->mRenderPipeline->GetLabel().c_str());

    TRACE_EVENT_FLOW_BEGIN1(device->GetPlatform(), General,
                            "CreateRenderPipelineAsyncTask::RunAsync", task.get(), "label",
                            eventLabel);

    // Using "taskPtr = std::move(task)" causes compilation error while it should be supported
    // since C++14:
    // https://docs.microsoft.com/en-us/cpp/cpp/lambda-expressions-in-cpp?view=msvc-160
    auto asyncTask = [taskPtr = task.release()] {
        std::unique_ptr<CreateRenderPipelineAsyncTask> innerTaskPtr(taskPtr);
        innerTaskPtr->Run();
    };

    device->GetAsyncTaskManager()->PostTask(std::move(asyncTask));
}
}  // namespace dawn::native
