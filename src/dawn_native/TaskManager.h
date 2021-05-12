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

#ifndef DAWNNATIVE_TASKMANAGER_H_
#define DAWNNATIVE_TASKMANAGER_H_

#include "common/RefCounted.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace dawn_platform {
    class WorkerTaskPool;
    class WaitableEvent;
}  // namespace dawn_platform

namespace dawn_native {

    class TaskManager {
      public:
        explicit TaskManager(dawn_platform::WorkerTaskPool* pool);
        ~TaskManager();

        // Task is a handle to a task returned from PostTask, but nothing
        // can be done with it right now. We could consider adding functions to
        // query the status or "steal" it.
        class Task : public RefCounted {
          public:
            ~Task();

          private:
            friend class TaskManager;
            TaskManager* manager;
            std::function<void()> fn;
            std::unique_ptr<dawn_platform::WaitableEvent> waitableEvent;
        };

        Ref<Task> PostTask(std::function<void()> fn);
        bool HasPendingTasks() const;
        void WaitForTasks();

      private:
        dawn_platform::WorkerTaskPool* mPool;
        std::mutex mMutex;
        std::unordered_map<Task*, Ref<Task>> mPendingTasks;
        std::atomic<uint64_t> mNumPendingTasks;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_TASKMANAGER_H_