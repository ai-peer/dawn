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

#ifndef DAWNNATIVE_CREATEPIPELINEASYNCTRACKER_H_
#define DAWNNATIVE_CREATEPIPELINEASYNCTRACKER_H_

#include "common/RefCounted.h"
#include "common/SerialQueue.h"
#include "dawn/webgpu.h"
#include "dawn_native/IntegerTypes.h"
#include "dawn_native/wgpu_structs_autogen.h"
#include "dawn_platform/DawnPlatform.h"

#include <list>
#include <memory>
#include <mutex>
#include <string>

namespace dawn_native {

    class ComputePipelineBase;
    class DeviceBase;
    class RenderPipelineBase;

    struct CreatePipelineAsyncTaskResultBase {
        CreatePipelineAsyncTaskResultBase(std::string errorMessage, void* userData);
        virtual ~CreatePipelineAsyncTaskResultBase();

        virtual void Finish() = 0;
        virtual void HandleShutDown() = 0;
        virtual void HandleDeviceLoss() = 0;

      protected:
        std::string mErrorMessage;
        void* mUserData;
    };

    struct CreateComputePipelineAsyncTaskResult final : public CreatePipelineAsyncTaskResultBase {
        CreateComputePipelineAsyncTaskResult(Ref<ComputePipelineBase> pipeline,
                                             std::string errorMessage,
                                             WGPUCreateComputePipelineAsyncCallback callback,
                                             void* userdata);

        void Finish() final;
        void HandleShutDown() final;
        void HandleDeviceLoss() final;

      private:
        Ref<ComputePipelineBase> mPipeline;
        WGPUCreateComputePipelineAsyncCallback mCreateComputePipelineAsyncCallback;
    };

    struct CreateRenderPipelineAsyncTaskResult final : public CreatePipelineAsyncTaskResultBase {
        CreateRenderPipelineAsyncTaskResult(Ref<RenderPipelineBase> pipeline,
                                            std::string errorMessage,
                                            WGPUCreateRenderPipelineAsyncCallback callback,
                                            void* userdata);

        void Finish() final;
        void HandleShutDown() final;
        void HandleDeviceLoss() final;

      private:
        Ref<RenderPipelineBase> mPipeline;
        WGPUCreateRenderPipelineAsyncCallback mCreateRenderPipelineAsyncCallback;
    };

    class CreatePipelineAsyncResultTracker {
      public:
        explicit CreatePipelineAsyncResultTracker(DeviceBase* device);
        ~CreatePipelineAsyncResultTracker();

        void TrackTask(std::unique_ptr<CreatePipelineAsyncTaskResultBase> task,
                       ExecutionSerial serial);
        void Tick(ExecutionSerial finishedSerial);
        void ClearForShutDown();

      private:
        std::vector<std::unique_ptr<CreatePipelineAsyncTaskResultBase>>
        GetTaskResultsBeforeFinishedSerial(ExecutionSerial finishedSerial);

        DeviceBase* mDevice;

        // All the operations on mCreatePipelineAsyncTaskResultsInFlight should be thread-safe.
        std::mutex mMutex;
        SerialQueue<ExecutionSerial, std::unique_ptr<CreatePipelineAsyncTaskResultBase>>
            mCreatePipelineAsyncTaskResultsInFlight;
    };

    class WorkerTask {
      public:
        virtual ~WorkerTask() = default;
        static void DoTask(void* userdata);

      private:
        virtual void Run() = 0;
    };

    class ComputePipelineAsyncTask : public WorkerTask {
      public:
        ComputePipelineAsyncTask(DeviceBase* device,
                                 const ComputePipelineDescriptor* descriptor,
                                 size_t blueprintHash,
                                 WGPUCreateComputePipelineAsyncCallback callback,
                                 void* userdata);

      private:
        void Run() override;

        DeviceBase* mDevice;
        size_t mBlueprintHash;
        WGPUCreateComputePipelineAsyncCallback mCallback;
        void* mUserData;

        struct ComboComputePipelineDescriptor : public ComputePipelineDescriptor {
            explicit ComboComputePipelineDescriptor(const ComputePipelineDescriptor* descriptor);

            std::string mLabel;
            Ref<PipelineLayoutBase> mPipelineLayout;
            Ref<ShaderModuleBase> mComputeShaderModule;
            std::string mEntryPoint;
        } mDescriptor;
    };

    class CreatePipelineAsyncTaskManager {
      public:
        explicit CreatePipelineAsyncTaskManager(DeviceBase* device);

        void StartComputePipelineAsyncWaitableTask(const ComputePipelineDescriptor* descriptor,
                                                   size_t blueprintHash,
                                                   WGPUCreateComputePipelineAsyncCallback callback,
                                                   void* userdata);
        void Tick();

        void WaitAndRemoveAll();

        bool HasWaitableTasksInFlight();

      private:
        DeviceBase* mDevice;
        dawn_platform::WorkerTaskPool* mWorkerTaskPool;
        std::mutex mWaitableTasksMutex;
        std::list<std::unique_ptr<dawn_platform::WaitableEvent>> mWaitableEventsInFlight;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_CREATEPIPELINEASYNCTRACKER_H_
