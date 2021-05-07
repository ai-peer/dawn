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

    void WorkerThreadTask::DoTask(void* userdata) {
        std::unique_ptr<WorkerThreadTask> workerTaskPtr(static_cast<WorkerThreadTask*>(userdata));
        workerTaskPtr->Run();
    }

    WaitableEventManager::WaitableEventManager() : mTaskSerial(0) {
    }

    WaitableEventManager::~WaitableEventManager() {
        ASSERT(mWaitableEventsInFlight.empty());
    }

    size_t WaitableEventManager::NextTaskSerial() {
        ++mTaskSerial;
        return mTaskSerial;
    }

    void WaitableEventManager::TrackNewWaitableEvent(
        size_t taskSerial,
        std::unique_ptr<dawn_platform::WaitableEvent> waitableEvent) {
        ASSERT(mWaitableEventsInFlight.find(taskSerial) == mWaitableEventsInFlight.end());
        mWaitableEventsInFlight.insert(std::make_pair(taskSerial, std::move(waitableEvent)));
    }

    void WaitableEventManager::ClearCompletedWaitableEvent(size_t taskSerial) {
        auto iter = mWaitableEventsInFlight.find(taskSerial);
        ASSERT(iter != mWaitableEventsInFlight.end());
        mWaitableEventsInFlight.erase(iter);
    }

    void WaitableEventManager::WaitAndClearAllWaitableEvent() {
        for (auto& iter : mWaitableEventsInFlight) {
            iter.second->Wait();
        }
        mWaitableEventsInFlight.clear();
    }

}  // namespace dawn_native
