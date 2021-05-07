// Copyright 2021 The Dawn Authors
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

#include "dawn_native/CallbackTaskManager.h"

#include "common/Assert.h"
#include "dawn_native/Device.h"
#include "dawn_platform/DawnPlatform.h"

namespace dawn_native {

    CallbackTask::CallbackTask() : mSerial(0), mOptionalWaitableEventManager(nullptr) {
    }

    void CallbackTask::Finish() {
        FinishImpl();
        if (mOptionalWaitableEventManager != nullptr) {
            mOptionalWaitableEventManager->RemoveCompletedWaitableEvent(mSerial);
        }
    }

    bool CallbackTaskManager::IsEmpty() {
        std::lock_guard<std::mutex> lock(mCallbackTaskQueueMutex);
        return mCallbackTaskQueue.empty();
    }

    std::vector<std::unique_ptr<CallbackTask>> CallbackTaskManager::AcquireCallbackTasks() {
        std::lock_guard<std::mutex> lock(mCallbackTaskQueueMutex);

        std::vector<std::unique_ptr<CallbackTask>> allTasks;
        allTasks.swap(mCallbackTaskQueue);
        return allTasks;
    }

    void CallbackTaskManager::AddCallbackTask(std::unique_ptr<CallbackTask> callbackTask) {
        std::lock_guard<std::mutex> lock(mCallbackTaskQueueMutex);
        mCallbackTaskQueue.push_back(std::move(callbackTask));
    }

    WorkerThreadTask::WorkerThreadTask(WaitableEventManager* waitableEventManager,
                                       CallbackTaskManager* callbackTaskManager)
        : mWaitableEventManager(waitableEventManager), mCallbackTaskManager(callbackTaskManager) {
    }

    WorkerThreadTask::~WorkerThreadTask() = default;

    void WorkerThreadTask::DoTask(void* userdata) {
        std::unique_ptr<WorkerThreadTask> workerTaskPtr(static_cast<WorkerThreadTask*>(userdata));
        std::unique_ptr<CallbackTask> callbackTask = workerTaskPtr->Run();
        if (callbackTask) {
            callbackTask->mSerial = workerTaskPtr->mSerial;
            callbackTask->mOptionalWaitableEventManager = workerTaskPtr->mWaitableEventManager;
            workerTaskPtr->mCallbackTaskManager->AddCallbackTask(std::move(callbackTask));
        }
    }

    void WorkerThreadTask::Start(dawn_platform::WorkerTaskPool* workerTaskPool) {
        mWaitableEventManager->StartWorkerThreadTask(this, workerTaskPool);
    }

    WaitableEventManager::WaitableEventManager() : mSerial(0) {
    }

    WaitableEventManager::~WaitableEventManager() {
        ASSERT(mWaitableEventsInFlight.empty());
    }

    size_t WaitableEventManager::NextSerial() {
        ++mSerial;
        return mSerial;
    }

    void WaitableEventManager::StartWorkerThreadTask(
        WorkerThreadTask* task,
        dawn_platform::WorkerTaskPool* workerTaskPool) {
        size_t serial = NextSerial();
        task->mSerial = serial;
        std::unique_ptr<dawn_platform::WaitableEvent> waitableEvent =
            workerTaskPool->PostWorkerTask(WorkerThreadTask::DoTask, task);
        TrackWaitableEvent(serial, std::move(waitableEvent));
    }

    void WaitableEventManager::TrackWaitableEvent(
        size_t serial,
        std::unique_ptr<dawn_platform::WaitableEvent> waitableEvent) {
        ASSERT(mWaitableEventsInFlight.find(serial) == mWaitableEventsInFlight.end());
        mWaitableEventsInFlight.insert(std::make_pair(serial, std::move(waitableEvent)));
    }

    void WaitableEventManager::RemoveCompletedWaitableEvent(size_t serial) {
        ASSERT(mWaitableEventsInFlight.find(serial) != mWaitableEventsInFlight.end());
        mWaitableEventsInFlight.erase(serial);
    }

    void WaitableEventManager::WaitAndClearAllWaitableEvent() {
        for (auto& iter : mWaitableEventsInFlight) {
            iter.second->Wait();
        }
        mWaitableEventsInFlight.clear();
    }

    bool WaitableEventManager::HasWaitableEventsInFlight() const {
        return !mWaitableEventsInFlight.empty();
    }

}  // namespace dawn_native
