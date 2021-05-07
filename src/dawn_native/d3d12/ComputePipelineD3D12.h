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

#include "dawn_native/CallbackTaskManager.h"
#include "dawn_native/d3d12/d3d12_platform.h"

namespace dawn_native {
    struct CreateComputePipelineAsyncWaitableCallbackTask;

    namespace d3d12 {

        class CreateComputePipelineAsyncTask;
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
            ~ComputePipeline() override;
            using ComputePipelineBase::ComputePipelineBase;

            friend class CreateComputePipelineAsyncTask;
            MaybeError Initialize(const ComputePipelineDescriptor* descriptor);
            ComPtr<ID3D12PipelineState> mPipelineState;
        };

        class CreateComputePipelineAsyncTask : public WorkerThreadTask {
          public:
            CreateComputePipelineAsyncTask(Device* device,
                                           ExecutionSerial taskSerial,
                                           const ComputePipelineDescriptor* descriptor,
                                           size_t blueprintHash,
                                           WGPUCreateComputePipelineAsyncCallback callback,
                                           void* userdata);

          private:
            void Run() override;

            Device* mDevice;

            Ref<ComputePipelineBase> mComputePipeline;
            ExecutionSerial mTaskSerial;
            size_t mBlueprintHash;
            WGPUCreateComputePipelineAsyncCallback mCallback;
            void* mUserdata;

            std::string mEntryPoint;
            Ref<ShaderModuleBase> mComputeShaderModule;
        };

    }  // namespace d3d12
}  // namespace dawn_native

#endif  // DAWNNATIVE_D3D12_COMPUTEPIPELINED3D12_H_
