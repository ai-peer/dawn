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

    CreatePipelineAsyncTaskResultBase::CreatePipelineAsyncTaskResultBase(std::string errorMessage,
                                                                         void* userdata)
        : mErrorMessage(errorMessage), mUserData(userdata) {
    }

    CreatePipelineAsyncTaskResultBase::~CreatePipelineAsyncTaskResultBase() {
    }

    CreateComputePipelineAsyncTaskResult::CreateComputePipelineAsyncTaskResult(
        Ref<ComputePipelineBase> pipeline,
        std::string errorMessage,
        WGPUCreateComputePipelineAsyncCallback callback,
        void* userdata)
        : CreatePipelineAsyncTaskResultBase(errorMessage, userdata),
          mPipeline(std::move(pipeline)),
          mCreateComputePipelineAsyncCallback(callback) {
    }

    void CreateComputePipelineAsyncTaskResult::Finish() {
        ASSERT(mCreateComputePipelineAsyncCallback != nullptr);

        if (mPipeline.Get() != nullptr) {
            mCreateComputePipelineAsyncCallback(
                WGPUCreatePipelineAsyncStatus_Success,
                reinterpret_cast<WGPUComputePipeline>(mPipeline.Detach()), "", mUserData);
        } else {
            mCreateComputePipelineAsyncCallback(WGPUCreatePipelineAsyncStatus_Error, nullptr,
                                                mErrorMessage.c_str(), mUserData);
        }
    }

    void CreateComputePipelineAsyncTaskResult::HandleShutDown() {
        ASSERT(mCreateComputePipelineAsyncCallback != nullptr);

        mCreateComputePipelineAsyncCallback(WGPUCreatePipelineAsyncStatus_DeviceDestroyed, nullptr,
                                            "Device destroyed before callback", mUserData);
    }

    void CreateComputePipelineAsyncTaskResult::HandleDeviceLoss() {
        ASSERT(mCreateComputePipelineAsyncCallback != nullptr);

        mCreateComputePipelineAsyncCallback(WGPUCreatePipelineAsyncStatus_DeviceLost, nullptr,
                                            "Device lost before callback", mUserData);
    }

    CreateRenderPipelineAsyncTaskResult::CreateRenderPipelineAsyncTaskResult(
        Ref<RenderPipelineBase> pipeline,
        std::string errorMessage,
        WGPUCreateRenderPipelineAsyncCallback callback,
        void* userdata)
        : CreatePipelineAsyncTaskResultBase(errorMessage, userdata),
          mPipeline(std::move(pipeline)),
          mCreateRenderPipelineAsyncCallback(callback) {
    }

    void CreateRenderPipelineAsyncTaskResult::Finish() {
        ASSERT(mCreateRenderPipelineAsyncCallback != nullptr);

        if (mPipeline.Get() != nullptr) {
            mCreateRenderPipelineAsyncCallback(
                WGPUCreatePipelineAsyncStatus_Success,
                reinterpret_cast<WGPURenderPipeline>(mPipeline.Detach()), "", mUserData);
        } else {
            mCreateRenderPipelineAsyncCallback(WGPUCreatePipelineAsyncStatus_Error, nullptr,
                                               mErrorMessage.c_str(), mUserData);
        }
    }

    void CreateRenderPipelineAsyncTaskResult::HandleShutDown() {
        ASSERT(mCreateRenderPipelineAsyncCallback != nullptr);

        mCreateRenderPipelineAsyncCallback(WGPUCreatePipelineAsyncStatus_DeviceDestroyed, nullptr,
                                           "Device destroyed before callback", mUserData);
    }

    void CreateRenderPipelineAsyncTaskResult::HandleDeviceLoss() {
        ASSERT(mCreateRenderPipelineAsyncCallback != nullptr);

        mCreateRenderPipelineAsyncCallback(WGPUCreatePipelineAsyncStatus_DeviceLost, nullptr,
                                           "Device lost before callback", mUserData);
    }

    CreatePipelineAsyncResultTracker::CreatePipelineAsyncResultTracker(DeviceBase* device)
        : mDevice(device) {
    }

    CreatePipelineAsyncResultTracker::~CreatePipelineAsyncResultTracker() {
        ASSERT(mCreatePipelineAsyncTaskResultsInFlight.Empty());
    }

    void CreatePipelineAsyncResultTracker::TrackTask(
        std::unique_ptr<CreatePipelineAsyncTaskResultBase> task,
        ExecutionSerial serial) {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCreatePipelineAsyncTaskResultsInFlight.Enqueue(std::move(task), serial);
        }
        mDevice->AddFutureSerial(serial);
    }

    void CreatePipelineAsyncResultTracker::Tick(ExecutionSerial finishedSerial) {
        // If a user calls Queue::Submit inside Create*PipelineAsync, then the device will be
        // ticked, which in turns ticks the tracker, causing reentrance here. To prevent the
        // reentrant call from invalidating mCreatePipelineAsyncTasksInFlight while in use by the
        // first call, we remove the tasks to finish from the queue, update
        // mCreatePipelineAsyncTasksInFlight, then run the callbacks.
        std::vector<std::unique_ptr<CreatePipelineAsyncTaskResultBase>> tasks;
        {
            // lock must be released before task->Finish() in case the deadlock happens when the
            // callback calls device->Tick() in task->Finish().
            std::lock_guard<std::mutex> lock(mMutex);
            for (auto& task : mCreatePipelineAsyncTaskResultsInFlight.IterateUpTo(finishedSerial)) {
                tasks.push_back(std::move(task));
            }
            mCreatePipelineAsyncTaskResultsInFlight.ClearUpTo(finishedSerial);
        }

        for (auto& task : tasks) {
            if (mDevice->IsLost()) {
                task->HandleDeviceLoss();
            } else {
                task->Finish();
            }
        }
    }

    void CreatePipelineAsyncResultTracker::ClearForShutDown() {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (auto& task : mCreatePipelineAsyncTaskResultsInFlight.IterateAll()) {
                task->HandleShutDown();
            }
        }

        mCreatePipelineAsyncTaskResultsInFlight.Clear();
    }

    CreateComputePipelineAsyncWaitableTask::CreateComputePipelineAsyncWaitableTask(
        DeviceBase* device,
        dawn_platform::WorkerTaskPool* workerTaskPool,
        const ComputePipelineDescriptor* descriptor,
        size_t blueprintHash,
        WGPUCreateComputePipelineAsyncCallback callback,
        void* userdata)
        : mDevice(device),
          mDescriptor(*descriptor),
          mBlueprintHash(blueprintHash),
          mCallback(callback),
          mUserData(userdata),
          mPipelineLayout(descriptor->layout),
          mComputeShaderModule(descriptor->computeStage.module) {
        mDescriptor.layout = mPipelineLayout.Get();
        mDescriptor.computeStage.module = mComputeShaderModule.Get();

        ASSERT(workerTaskPool != nullptr);
        mWaitableEvent = workerTaskPool->PostWorkerTask(DoTaskOnWorkerTaskPool, this);
    }

    bool CreateComputePipelineAsyncWaitableTask::IsComplete() const {
        return mWaitableEvent->IsComplete();
    }

    void CreateComputePipelineAsyncWaitableTask::Wait() {
        mWaitableEvent->Wait();
    }

    void CreateComputePipelineAsyncWaitableTask::DoTaskOnWorkerTaskPool(void* waitableTask) {
        CreateComputePipelineAsyncWaitableTask* taskPtr =
            static_cast<CreateComputePipelineAsyncWaitableTask*>(waitableTask);
        taskPtr->DoTask();
    }

    void CreateComputePipelineAsyncWaitableTask::DoTask() {
        ASSERT(mDevice != nullptr);

        // This class is declared as a friend of class DeviceBase so we can use its private member
        // functions.
        mDevice->CreateComputePipelineAsyncImplBase(&mDescriptor, mBlueprintHash, mCallback,
                                                    mUserData);
    }

    CreatePipelineAsyncTaskManager::CreatePipelineAsyncTaskManager(DeviceBase* device)
        : mDevice(device), mWorkerTaskPool(device->GetOrCreateWorkerTaskPool()) {
    }

    void CreatePipelineAsyncTaskManager::StartCreateComputePipelineAsyncWaitableTask(
        const ComputePipelineDescriptor* descriptor,
        size_t blueprintHash,
        WGPUCreateComputePipelineAsyncCallback callback,
        void* userdata) {
        std::lock_guard<std::mutex> lock(mWaitableTasksMutex);
        mWaitableTasksInFlight.emplace_back(mDevice, mWorkerTaskPool, descriptor, blueprintHash,
                                            callback, userdata);
    }

    void CreatePipelineAsyncTaskManager::Tick() {
        std::lock_guard<std::mutex> lock(mWaitableTasksMutex);
        auto iter = mWaitableTasksInFlight.begin();
        while (iter != mWaitableTasksInFlight.end()) {
            if (iter->IsComplete()) {
                iter = mWaitableTasksInFlight.erase(iter);
            } else {
                ++iter;
            }
        }
    }

    void CreatePipelineAsyncTaskManager::WaitAll() {
        std::lock_guard<std::mutex> lock(mWaitableTasksMutex);
        for (auto iter = mWaitableTasksInFlight.begin(); iter != mWaitableTasksInFlight.end();
             ++iter) {
            iter->Wait();
        }
    }

}  // namespace dawn_native
