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

    WorkerThreadTask::WorkerThreadTask(DeviceBase* device)
        : mDevice(device), mWaitableEvent(nullptr) {
    }

    WorkerThreadTask::~WorkerThreadTask() {
        ASSERT(mWaitableEvent != nullptr);
        mDevice->GetWaitableEventManager()->AddCompletedWaitableEvent(mWaitableEvent);
    }

    void WorkerThreadTask::DoTask(void* userdata) {
        std::unique_ptr<WorkerThreadTask> workerTaskPtr(static_cast<WorkerThreadTask*>(userdata));
        workerTaskPtr->Run();
    }

    void WorkerThreadTask::Start() {
        std::unique_ptr<dawn_platform::WaitableEvent> waitableEvent =
            mDevice->GetWorkerTaskPool()->PostWorkerTask(DoTask, this);
        mWaitableEvent = waitableEvent.get();
        mDevice->GetWaitableEventManager()->TrackWaitableEvent(std::move(waitableEvent));
    }

    WaitableEventManager::~WaitableEventManager() {
        {
            std::lock_guard<std::mutex> lock(mCompletedWaitableEventQueueMutex);
            ASSERT(mCompletedWaitableEventQueue.empty());
        }

        ASSERT(mWaitableEventsInFlight.empty());
    }

    void WaitableEventManager::TrackWaitableEvent(
        std::unique_ptr<dawn_platform::WaitableEvent> waitableEvent) {
        ASSERT(mWaitableEventsInFlight.find(waitableEvent.get()) == mWaitableEventsInFlight.end());
        mWaitableEventsInFlight.insert(
            std::make_pair(waitableEvent.get(), std::move(waitableEvent)));
    }

    bool WaitableEventManager::HasCompletedWaitableEvent() {
        std::lock_guard<std::mutex> lock(mCompletedWaitableEventQueueMutex);
        return !mCompletedWaitableEventQueue.empty();
    }

    void WaitableEventManager::AddCompletedWaitableEvent(
        dawn_platform::WaitableEvent* waitableEvent) {
        std::lock_guard<std::mutex> lock(mCompletedWaitableEventQueueMutex);
        mCompletedWaitableEventQueue.push_back(waitableEvent);
    }

    void WaitableEventManager::ClearAllCompletedWaitableEvents() {
        std::lock_guard<std::mutex> lock(mCompletedWaitableEventQueueMutex);
        for (dawn_platform::WaitableEvent* waitableEvent : mCompletedWaitableEventQueue) {
            ASSERT(mWaitableEventsInFlight.find(waitableEvent) != mWaitableEventsInFlight.end());
            mWaitableEventsInFlight.erase(waitableEvent);
        }
        mCompletedWaitableEventQueue.clear();
    }

    void WaitableEventManager::WaitAndClearAllWaitableEvent() {
        for (auto& iter : mWaitableEventsInFlight) {
            iter.second->Wait();
        }

        {
            std::lock_guard<std::mutex> lock(mCompletedWaitableEventQueueMutex);
            mCompletedWaitableEventQueue.clear();
        }
        mWaitableEventsInFlight.clear();
    }

    bool WaitableEventManager::HasWaitableEventsInFlight() const {
        return !mWaitableEventsInFlight.empty();
    }

}  // namespace dawn_native
