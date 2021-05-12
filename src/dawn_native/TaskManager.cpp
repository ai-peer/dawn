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

#include "dawn_native/TaskManager.h"

#include "dawn_platform/DawnPlatform.h"

namespace dawn_native {

    TaskManager::Task::~Task() = default;

    TaskManager::TaskManager(dawn_platform::WorkerTaskPool* pool) : mPool(pool) {
    }
    TaskManager::~TaskManager() = default;

    Ref<TaskManager::Task> TaskManager::PostTask(std::function<void()> fn) {
        mNumPendingTasks++;

        // If these allocations becomes expensive, we can slab-allocate tasks.
        auto task = AcquireRef(new Task());
        task->manager = this;
        task->fn = std::move(fn);

        // Ref the task since it is accessed inside the worker function.
        // The worker function will acquire and release the task upon completion.
        task->Reference();
        task->waitableEvent = mPool->PostWorkerTask(
            [](void* userdata) {
                Ref<Task> task = AcquireRef(static_cast<Task*>(userdata));
                std::move(task->fn)();
                {
                    std::lock_guard<std::mutex> lock(task->manager->mMutex);
                    task->manager->mPendingTasks.erase(
                        task->manager->mPendingTasks.find(task.Get()));
                }
                task->manager->mNumPendingTasks--;
            },
            task.Get());

        {
            std::lock_guard<std::mutex> lock(mMutex);
            mPendingTasks.emplace(task.Get(), task);
        }

        return task;
    }

    bool TaskManager::HasPendingTasks() const {
        return mNumPendingTasks.load() > 0u;
    }

    void TaskManager::WaitForTasks() {
        std::unordered_map<Task*, Ref<Task>> tasks;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            tasks = std::move(mPendingTasks);
        }

        for (auto&& it : tasks) {
            it.second->waitableEvent->Wait();
        }
    }

}  // namespace dawn_native
