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

#ifndef SRC_DAWN_NATIVE_CREATEPIPELINEASYNCTASK_H_
#define SRC_DAWN_NATIVE_CREATEPIPELINEASYNCTASK_H_

#include <memory>
#include <string>

#include "dawn/common/Ref.h"
#include "dawn/native/CallbackTaskManager.h"
#include "dawn/native/Error.h"
#include "dawn/native/EventManager.h"
#include "dawn/native/Pipeline.h"
#include "dawn/webgpu.h"
#include "partition_alloc/pointers/raw_ptr.h"

namespace dawn::native {

class ComputePipelineBase;
class DeviceBase;
class PipelineLayoutBase;
class RenderPipelineBase;
class ShaderModuleBase;
struct FlatComputePipelineDescriptor;
struct CreateComputePipelineAsyncEvent;

// CreateComputePipelineAsyncTask represents async tasks run on a separate thread.
class CreateComputePipelineAsyncTask {
  public:
    CreateComputePipelineAsyncTask(Ref<ComputePipelineBase> nonInitializedComputePipeline);
    ~CreateComputePipelineAsyncTask();

    void Run(CreateComputePipelineAsyncEvent* event);
    static void RunAsync(DeviceBase* device, CreateComputePipelineAsyncEvent* event);

  private:
    Ref<ComputePipelineBase> mPipeline;
    // Used to keep ShaderModuleBase::mTintProgram alive until pipeline initialization is done.
    PipelineBase::ScopedUseShaderPrograms mScopedUseShaderPrograms;
};

// CreateComputePipelineAsyncEvent represents the async event managed by event manager.
// It handles the inputs and outputs of CreateComputePipelineAsync()
// and the life time of the CreateComputePipelineAsyncTask.
struct CreateComputePipelineAsyncEvent final : public EventManager::TrackedEvent {
    WGPUCreateComputePipelineAsyncCallback mCallback;
    // TODO(https://crbug.com/2364): The pointer is dangling in
    // webgpu_cts_with_validation_tests. We should investigate, and decide if
    // this should be fixed, or turned into a DisableDanglingPtrDetection.
    raw_ptr<void, DanglingUntriaged> mUserdata;
    ResultOrError<Ref<ComputePipelineBase>> mPipelineOrError;
    std::unique_ptr<CreateComputePipelineAsyncTask> mTask;

    // Create an event backed by the given system event (for async pipeline creation goes through
    // the driver).
    CreateComputePipelineAsyncEvent(DeviceBase* device,
                                    const CreateComputePipelineAsyncCallbackInfo& callbackInfo,
                                    ResultOrError<Ref<ComputePipelineBase>> pipelineOrError,
                                    Ref<SystemEvent> systemEvent,
                                    std::unique_ptr<CreateComputePipelineAsyncTask> task);

    // Create an event that's ready at creation (for cached results, errors, etc.)
    CreateComputePipelineAsyncEvent(DeviceBase* device,
                                    const CreateComputePipelineAsyncCallbackInfo& callbackInfo,
                                    ResultOrError<Ref<ComputePipelineBase>> pipelineOrError);

    ~CreateComputePipelineAsyncEvent() override;

    void Complete(EventCompletionType completionType) override;
};

// CreateRenderPipelineAsyncTask defines all the inputs and outputs of
// CreateRenderPipelineAsync() tasks, which are the same among all the backends.
class CreateRenderPipelineAsyncTask {
  public:
    CreateRenderPipelineAsyncTask(Ref<RenderPipelineBase> nonInitializedRenderPipeline,
                                  WGPUCreateRenderPipelineAsyncCallback callback,
                                  void* userdata);
    ~CreateRenderPipelineAsyncTask();

    void Run();

    static void RunAsync(std::unique_ptr<CreateRenderPipelineAsyncTask> task);

  private:
    Ref<RenderPipelineBase> mRenderPipeline;
    WGPUCreateRenderPipelineAsyncCallback mCallback;
    raw_ptr<void> mUserdata;
    // Used to keep ShaderModuleBase::mTintProgram alive until pipeline initialization is done.
    PipelineBase::ScopedUseShaderPrograms mScopedUseShaderPrograms;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_CREATEPIPELINEASYNCTASK_H_
