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

#ifndef DAWNNATIVE_D3D12_COMPUTEPIPELINED3D12_H_
#define DAWNNATIVE_D3D12_COMPUTEPIPELINED3D12_H_

#include "dawn_native/ComputePipeline.h"

#include "dawn_native/d3d12/d3d12_platform.h"
#include "dawn_platform/DawnPlatform.h"

#include <list>
#include <mutex>

namespace dawn_native { namespace d3d12 {

    class CreateComputePipelineAsyncTask;
    class CreateComputePipelineAsyncTaskManager;
    class Device;

    class ComputePipeline final : public ComputePipelineBase {
      public:
        static ResultOrError<Ref<ComputePipeline>> Create(
            Device* device,
            const ComputePipelineDescriptor* descriptor);
        static void CreateAsync(Device* device,
                                const ComputePipelineDescriptor* descriptor,
                                size_t blueprintHash,
                                WGPUCreateComputePipelineAsyncCallback callback,
                                void* userdata);

        ComputePipeline() = delete;

        ID3D12PipelineState* GetPipelineState() const;

      private:
        friend class CreateComputePipelineAsyncTask;
        ~ComputePipeline() override;
        using ComputePipelineBase::ComputePipelineBase;
        MaybeError Initialize(const ComputePipelineDescriptor* descriptor);
        MaybeError Initialize(ShaderModuleBase* computeShaderModule, const char* entryPoint);
        ComPtr<ID3D12PipelineState> mPipelineState;
    };

    struct CreateComputePipelineAsyncResult {
        size_t blueprintHash;
        WGPUCreateComputePipelineAsyncCallback callback;
        void* userData;
        Ref<ComputePipeline> computePipeline;
        std::string errorMessage;
    };

    class CreateComputePipelineAsyncTask {
      public:
        CreateComputePipelineAsyncTask(DeviceBase* device,
                                       const ComputePipelineDescriptor* descriptor,
                                       size_t blueprintHash,
                                       WGPUCreateComputePipelineAsyncCallback callback,
                                       void* userdata);
        bool IsComplete() const;
        void Wait();

      private:
        friend class CreateComputePipelineAsyncTaskManager;
        static void DoTask(void* task);

        CreateComputePipelineAsyncResult mResult;
        std::string mEntryPoint;
        Ref<ShaderModuleBase> mComputeShaderModule;

        std::unique_ptr<dawn_platform::WaitableEvent> mWaitableEvent;
    };

    class CreateComputePipelineAsyncTaskManager {
      public:
        explicit CreateComputePipelineAsyncTaskManager(DeviceBase* device);

        void StartComputePipelineAsyncWaitableTask(const ComputePipelineDescriptor* descriptor,
                                                   size_t blueprintHash,
                                                   WGPUCreateComputePipelineAsyncCallback callback,
                                                   void* userdata);
        std::list<CreateComputePipelineAsyncResult> GetCompletedCreateComputePipelineAsyncTasks();
        std::list<CreateComputePipelineAsyncResult> WaitAndGetAllCreateComputePipelineAsyncTasks();
        bool HasTasksInFlight();

      private:
        DeviceBase* mDevice;
        std::mutex mMutex;
        std::list<std::unique_ptr<CreateComputePipelineAsyncTask>> mTasksInFlight;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_COMPUTEPIPELINED3D12_H_
