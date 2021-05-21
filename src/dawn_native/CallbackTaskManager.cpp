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

    void WorkerThreadTaskManager::WaitableTask::SetWaitableEvent(
        std::unique_ptr<dawn_platform::WaitableEvent> waitableEvent) {
        {
            // mWaitableEvent must be set in main thread(PostTask()) before it is accessed in
            // sub-thread (DoWaitableTask()), so we should protect it with a condition_variable and
            // a mutex.
            std::lock_guard<std::mutex> lock(mWaitableEventMutex);
            mWaitableEvent = std::move(waitableEvent);
        }
        mWaitableEventConditionVariable.notify_all();
    }

    dawn_platform::WaitableEvent* WorkerThreadTaskManager::WaitableTask::GetWaitableEvent() {
        std::unique_lock<std::mutex> lock(mWaitableEventMutex);
        mWaitableEventConditionVariable.wait(lock, [this] { return mWaitableEvent != nullptr; });
        return mWaitableEvent.get();
    }

    WorkerThreadTask::~WorkerThreadTask() = default;

    WorkerThreadTaskManager::WorkerThreadTaskManager(dawn_platform::WorkerTaskPool* workerTaskPool)
        : mWorkerTaskPool(workerTaskPool) {
    }

    void WorkerThreadTaskManager::PostTask(std::unique_ptr<WorkerThreadTask> workerThreadTask) {
        // If these allocations becomes expensive, we can slab-allocate tasks.
        Ref<WaitableTask> waitableTask = AcquireRef(new WaitableTask());
        waitableTask->taskManager = this;
        waitableTask->task = std::move(workerThreadTask);

        {
            // We insert new waitableTask objects into mPendingTasks in main thread (PostTask()),
            // and we may remove waitableTask objects from mPendingTasks in either main thread
            // (WaitAllPendingTasks()) or sub-thread (TaskCompleted), so mPendingTasks should be
            // protected by a mutex.
            std::lock_guard<std::mutex> lock(mPendingTasksMutex);
            mPendingTasks.emplace(waitableTask->task.get(), waitableTask);
        }

        // Ref the task since it is accessed inside the worker function.
        // The worker function will acquire and release the task upon completion.
        waitableTask->Reference();
        std::unique_ptr<dawn_platform::WaitableEvent> waitableEvent =
            mWorkerTaskPool->PostWorkerTask(DoWaitableTask, waitableTask.Get());
        waitableTask->SetWaitableEvent(std::move(waitableEvent));
    }

    void WorkerThreadTaskManager::TaskCompleted(WorkerThreadTask* task) {
        std::lock_guard<std::mutex> lock(mPendingTasksMutex);
        auto iter = mPendingTasks.find(task);
        if (iter != mPendingTasks.end()) {
            mPendingTasks.erase(iter);
        }
    }

    void WorkerThreadTaskManager::WaitAllPendingTasks() {
        std::unordered_map<WorkerThreadTask*, Ref<WaitableTask>> allPendingTasks;

        {
            std::lock_guard<std::mutex> lock(mPendingTasksMutex);
            allPendingTasks.swap(mPendingTasks);
        }

        for (auto& keyValue : allPendingTasks) {
            keyValue.second->GetWaitableEvent()->Wait();
        }
    }

    void WorkerThreadTaskManager::DoWaitableTask(void* task) {
        Ref<WaitableTask> waitableTask = AcquireRef(static_cast<WaitableTask*>(task));
        waitableTask->task->Run();
        waitableTask->taskManager->TaskCompleted(waitableTask->task.get());
        waitableTask->GetWaitableEvent()->MarkAsComplete();
    }

}  // namespace dawn_native
