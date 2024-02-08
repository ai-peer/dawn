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
    Ref<ComputePipelineBase> nonInitializedComputePipeline,
    WGPUCreateComputePipelineAsyncCallback callback,
    void* userdata)
    : mComputePipeline(std::move(nonInitializedComputePipeline)),
      //   mCallback(callback),
      mUserdata(userdata),
      mScopedUseShaderPrograms(mComputePipeline->UseShaderPrograms()) {
    DAWN_ASSERT(mComputePipeline != nullptr);
}

CreateComputePipelineAsyncTask::~CreateComputePipelineAsyncTask() = default;

// void CreateComputePipelineAsyncTask::Run() {
void CreateComputePipelineAsyncTask::Run(CreateComputePipelineAsyncEvent* event) {
    // completionEvent->Signal();

    const char* eventLabel = utils::GetLabelForTrace(mComputePipeline->GetLabel().c_str());

    DeviceBase* device = mComputePipeline->GetDevice();
    TRACE_EVENT_FLOW_END1(device->GetPlatform(), General,
                          "CreateComputePipelineAsyncTask::RunAsync", this, "label", eventLabel);
    TRACE_EVENT1(device->GetPlatform(), General, "CreateComputePipelineAsyncTask::Run", "label",
                 eventLabel);

    MaybeError maybeError;
    {
        SCOPED_DAWN_HISTOGRAM_TIMER_MICROS(device->GetPlatform(), "CreateComputePipelineUS");
        maybeError = mComputePipeline->Initialize(std::move(mScopedUseShaderPrograms));
    }
    DAWN_HISTOGRAM_BOOLEAN(device->GetPlatform(), "CreateComputePipelineSuccess",
                           maybeError.IsSuccess());
    // if (maybeError.IsError()) {
    //     device->AddComputePipelineAsyncCallbackTask(
    //         maybeError.AcquireError(), mComputePipeline->GetLabel().c_str(), mCallback,
    //         mUserdata);
    // } else {
    //     device->AddComputePipelineAsyncCallbackTask(mComputePipeline, mCallback, mUserdata);
    // }

    std::get<Ref<SystemEvent>>(event->GetCompletionData())->Signal();
    // event->Complete(EventCompletionType::Ready);
}

// void CreateComputePipelineAsyncTask::RunAsync(
//     std::unique_ptr<CreateComputePipelineAsyncTask> task) {
//     DeviceBase* device = task->mComputePipeline->GetDevice();

//     const char* eventLabel = utils::GetLabelForTrace(task->mComputePipeline->GetLabel().c_str());

//     TRACE_EVENT_FLOW_BEGIN1(device->GetPlatform(), General,
//                             "CreateComputePipelineAsyncTask::RunAsync", task.get(), "label",
//                             eventLabel);

//     // Using "taskPtr = std::move(task)" causes compilation error while it should be supported
//     // since C++14:
//     // https://docs.microsoft.com/en-us/cpp/cpp/lambda-expressions-in-cpp?view=msvc-160
//     auto asyncTask = [taskPtr = task.release()] {
//         std::unique_ptr<CreateComputePipelineAsyncTask> innnerTaskPtr(taskPtr);
//         innnerTaskPtr->Run();
//     };

//     device->GetAsyncTaskManager()->PostTask(std::move(asyncTask));
// }

void CreateComputePipelineAsyncTask::RunAsync(
    // std::unique_ptr<CreateComputePipelineAsyncTask> task
    // Ref<CreateComputePipelineAsyncEvent> event
    CreateComputePipelineAsyncEvent* event) {
    DeviceBase* device = event->mTask->mComputePipeline->GetDevice();

    const char* eventLabel =
        utils::GetLabelForTrace(event->mTask->mComputePipeline->GetLabel().c_str());

    TRACE_EVENT_FLOW_BEGIN1(device->GetPlatform(), General,
                            "CreateComputePipelineAsyncTask::RunAsync", event->mTask.get(), "label",
                            eventLabel);

    // // Using "taskPtr = std::move(task)" causes compilation error while it should be supported
    // // since C++14:
    // // https://docs.microsoft.com/en-us/cpp/cpp/lambda-expressions-in-cpp?view=msvc-160
    // auto asyncTask = [taskPtr = task.release()] {
    //     std::unique_ptr<CreateComputePipelineAsyncTask> innnerTaskPtr(taskPtr);
    //     innnerTaskPtr->Run();
    // };
    // auto asyncTask = [event = event, taskPtr = event->mTask.release()] {
    //     std::unique_ptr<CreateComputePipelineAsyncTask> innnerTaskPtr(taskPtr);
    //     innnerTaskPtr->Run(event);
    // };
    auto asyncTask = [event = event] { event->mTask->Run(event); };
    // auto asyncTask = [&] { event->mTask->Run(event); };
    device->GetAsyncTaskManager()->PostTask(std::move(asyncTask));
}

CreateComputePipelineAsyncEvent::CreateComputePipelineAsyncEvent(
    DeviceBase* device,
    const CreateComputePipelineAsyncCallbackInfo& callbackInfo,
    ResultOrError<Ref<ComputePipelineBase>> pipelineOrError,
    Ref<SystemEvent> systemEvent,
    std::unique_ptr<CreateComputePipelineAsyncTask> task)
    : TrackedEvent(callbackInfo.mode, systemEvent),
      mCallback(callbackInfo.callback),
      mUserdata(callbackInfo.userdata),
      mPipelineOrError(std::move(pipelineOrError)),
      //   mSystemEvent(std::move(systemEvent))
      mTask(std::move(task)) {
    // TODO
    // TRACE_EVENT_ASYNC_BEGIN0(device->GetPlatform(), General,
    //     "Device::APICreateComputePipelineAsync",
    //     uint64_t(serial));
}

// Create an event that's ready at creation (for cached results, errors, etc.)
CreateComputePipelineAsyncEvent::CreateComputePipelineAsyncEvent(
    DeviceBase* device,
    const CreateComputePipelineAsyncCallbackInfo& callbackInfo,
    ResultOrError<Ref<ComputePipelineBase>> pipelineOrError)
    : TrackedEvent(callbackInfo.mode, TrackedEvent::Completed{}),
      //   TrackedEvent(callbackInfo.mode, device->GetQueue(), kBeginningOfGPUTime),
      mCallback(callbackInfo.callback),
      mUserdata(callbackInfo.userdata),
      mPipelineOrError(std::move(pipelineOrError)) {
    // TRACE_EVENT_ASYNC_BEGIN0(device->GetPlatform(), General,
    //     "Device::APICreateComputePipelineAsync",
    //     kBeginningOfGPUTime);
    // if error validation error?
    CompleteIfSpontaneous();
}

CreateComputePipelineAsyncEvent::~CreateComputePipelineAsyncEvent() {
    EnsureComplete(EventCompletionType::Shutdown);
}

void CreateComputePipelineAsyncEvent::Complete(EventCompletionType completionType) {
    // TODO

    // if (const auto* queueAndSerial = std::get_if<QueueAndSerial>(&GetCompletionData())) {
    //     TRACE_EVENT_ASYNC_END0(queueAndSerial->queue->GetDevice()->GetPlatform(), General,
    //                         "Device::APICreateComputePipelineAsync",
    //                         uint64_t(queueAndSerial->completionSerial));
    // }

    if (completionType == EventCompletionType::Shutdown) {
        mCallback(ToAPI(wgpu::CreatePipelineAsyncStatus::InstanceDropped), nullptr, nullptr,
                  mUserdata);
        return;
    }

    // ready

    if (mPipelineOrError.IsError()) {
        std::unique_ptr<ErrorData> errorData = mPipelineOrError.AcquireError();
        wgpu::CreatePipelineAsyncStatus status;
        switch (errorData->GetType()) {
            case InternalErrorType::Validation:
                status = wgpu::CreatePipelineAsyncStatus::ValidationError;
                break;
            default:
                status = wgpu::CreatePipelineAsyncStatus::InternalError;
                break;
        }
        mCallback(ToAPI(status), nullptr, errorData->GetFormattedMessage().c_str(), mUserdata);
        return;
    }

    // Is pipeline
    const char* message = "";  // temp
    mCallback(ToAPI(wgpu::CreatePipelineAsyncStatus::Success),
              // ToAPI(ReturnToAPI(std::move(mPipelineOrError.AcquireSuccess()))),
              ToAPI(ReturnToAPI(mPipelineOrError.AcquireSuccess())), message, mUserdata);
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
