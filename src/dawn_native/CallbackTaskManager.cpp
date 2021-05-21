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
            std::lock_guard<std::mutex> lock(mPendingTasksMutex);
            mPendingTasks.emplace(waitableTask->task.get(), waitableTask);
        }

        // Ref the task since it is accessed inside the worker function.
        // The worker function will acquire and release the task upon completion.
        waitableTask->Reference();
        waitableTask->waitableEvent =
            mWorkerTaskPool->PostWorkerTask(DoWaitableTask, waitableTask.Get());
    }

    void WorkerThreadTaskManager::TaskCompleted(WorkerThreadTask* task) {
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
            keyValue.second->waitableEvent->Wait();
        }
    }

    void WorkerThreadTaskManager::DoWaitableTask(void* task) {
        Ref<WaitableTask> waitableTask = AcquireRef(static_cast<WaitableTask*>(task));
        waitableTask->task->Run();
        waitableTask->taskManager->TaskCompleted(waitableTask->task.get());
        waitableTask->waitableEvent->MarkAsComplete();
    }

}  // namespace dawn_native
