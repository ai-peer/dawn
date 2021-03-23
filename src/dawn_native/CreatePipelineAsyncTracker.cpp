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

#include "dawn_native/CreatePipelineAsyncTracker.h"

#include "dawn_native/ComputePipeline.h"
#include "dawn_native/Device.h"
#include "dawn_native/RenderPipeline.h"

namespace dawn_native {

    CreatePipelineAsyncTaskBase::CreatePipelineAsyncTaskBase(std::string errorMessage,
                                                             void* userdata)
        : mErrorMessage(errorMessage), mUserData(userdata) {
    }

    CreatePipelineAsyncTaskBase::~CreatePipelineAsyncTaskBase() {
    }

    CreateComputePipelineAsyncTask::CreateComputePipelineAsyncTask(
        ComputePipelineBase* pipeline,
        std::string errorMessage,
        WGPUCreateComputePipelineAsyncCallback callback,
        void* userdata)
        : CreatePipelineAsyncTaskBase(errorMessage, userdata),
          mPipeline(pipeline),
          mCreateComputePipelineAsyncCallback(callback) {
    }

    void CreateComputePipelineAsyncTask::FinishInTick() {
        ASSERT(mCreateComputePipelineAsyncCallback != nullptr);

        // TODO(jiawei.shao@intel.com): support handling device lost
        if (mPipeline) {
            mCreateComputePipelineAsyncCallback(WGPUCreatePipelineAsyncStatus_Success,
                                                reinterpret_cast<WGPUComputePipeline>(mPipeline),
                                                "", mUserData);
        } else {
            mCreateComputePipelineAsyncCallback(WGPUCreatePipelineAsyncStatus_Error, nullptr,
                                                mErrorMessage.c_str(), mUserData);
        }

        // Set mCreateComputePipelineAsyncCallback to nullptr in case it is called more than once.
        mCreateComputePipelineAsyncCallback = nullptr;
    }

    void CreateComputePipelineAsyncTask::FinishForShutDown() {
        ASSERT(mCreateComputePipelineAsyncCallback != nullptr);

        mCreateComputePipelineAsyncCallback(WGPUCreatePipelineAsyncStatus_DeviceDestroyed, nullptr,
                                            "Device destroyed before callback", mUserData);
        mPipeline->Release();

        // Set mCreateComputePipelineAsyncCallback to nullptr in case it is called more than once.
        mCreateComputePipelineAsyncCallback = nullptr;
    }

    CreateRenderPipelineAsyncTask::CreateRenderPipelineAsyncTask(
        RenderPipelineBase* pipeline,
        std::string errorMessage,
        WGPUCreateRenderPipelineAsyncCallback callback,
        void* userdata)
        : CreatePipelineAsyncTaskBase(errorMessage, userdata),
          mPipeline(pipeline),
          mCreateRenderPipelineAsyncCallback(callback) {
    }

    void CreateRenderPipelineAsyncTask::FinishInTick() {
        ASSERT(mCreateRenderPipelineAsyncCallback != nullptr);

        // TODO(jiawei.shao@intel.com): support handling device lost
        if (mPipeline != nullptr) {
            mCreateRenderPipelineAsyncCallback(WGPUCreatePipelineAsyncStatus_Success,
                                               reinterpret_cast<WGPURenderPipeline>(mPipeline), "",
                                               mUserData);
        } else {
            mCreateRenderPipelineAsyncCallback(WGPUCreatePipelineAsyncStatus_Error,
                                               reinterpret_cast<WGPURenderPipeline>(mPipeline),
                                               mErrorMessage.c_str(), mUserData);
        }

        // Set mCreatePipelineAsyncCallback to nullptr in case it is called more than once.
        mCreateRenderPipelineAsyncCallback = nullptr;
    }

    void CreateRenderPipelineAsyncTask::FinishForShutDown() {
        ASSERT(mCreateRenderPipelineAsyncCallback != nullptr);

        mCreateRenderPipelineAsyncCallback(WGPUCreatePipelineAsyncStatus_DeviceDestroyed, nullptr,
                                           "Device destroyed before callback", mUserData);
        mPipeline->Release();

        // Set mCreatePipelineAsyncCallback to nullptr in case it is called more than once.
        mCreateRenderPipelineAsyncCallback = nullptr;
    }

    CreatePipelineAsyncTracker::CreatePipelineAsyncTracker(DeviceBase* device) : mDevice(device) {
    }

    CreatePipelineAsyncTracker::~CreatePipelineAsyncTracker() {
        ASSERT(mCreatePipelineAsyncTasksInFlight.Empty());
    }

    void CreatePipelineAsyncTracker::TrackTask(std::unique_ptr<CreatePipelineAsyncTaskBase> task,
                                               ExecutionSerial serial) {
        mCreatePipelineAsyncTasksInFlight.Enqueue(std::move(task), serial);
        mDevice->AddFutureSerial(serial);
    }

    void CreatePipelineAsyncTracker::Tick(ExecutionSerial finishedSerial) {
        for (auto& task : mCreatePipelineAsyncTasksInFlight.IterateUpTo(finishedSerial)) {
            task->FinishInTick();
        }
        mCreatePipelineAsyncTasksInFlight.ClearUpTo(finishedSerial);
    }

    void CreatePipelineAsyncTracker::ClearForShutDown() {
        for (auto& task : mCreatePipelineAsyncTasksInFlight.IterateAll()) {
            task->FinishForShutDown();
        }
        mCreatePipelineAsyncTasksInFlight.Clear();
    }

}  // namespace dawn_native
