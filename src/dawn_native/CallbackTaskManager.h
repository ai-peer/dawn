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

#ifndef DAWNNATIVE_CALLBACK_TASK_MANAGER_H_
#define DAWNNATIVE_CALLBACK_TASK_MANAGER_H_

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "common/RefCounted.h"

namespace dawn_platform {
    class WaitableEvent;
    class WorkerTaskPool;
}  // namespace dawn_platform

namespace dawn_native {

    class CallbackTaskManager;

    struct CallbackTask {
      public:
        virtual ~CallbackTask() = default;
        virtual void Finish() = 0;
        virtual void HandleShutDown() = 0;
        virtual void HandleDeviceLoss() = 0;
    };

    class CallbackTaskManager {
      public:
        void AddCallbackTask(std::unique_ptr<CallbackTask> callbackTask);
        bool IsEmpty();
        std::vector<std::unique_ptr<CallbackTask>> AcquireCallbackTasks();

      private:
        std::mutex mCallbackTaskQueueMutex;
        std::vector<std::unique_ptr<CallbackTask>> mCallbackTaskQueue;
    };

    class WorkerThreadTask {
      public:
        virtual ~WorkerThreadTask();
        virtual void Run() = 0;
    };

    class WorkerThreadTaskManager {
      public:
        explicit WorkerThreadTaskManager(dawn_platform::WorkerTaskPool* workerTaskPool);

        void PostTask(std::unique_ptr<WorkerThreadTask> workerThreadTask);
        void TaskCompleted(WorkerThreadTask* task);
        void WaitAllPendingTasks();

      private:
        class WaitableTask : public RefCounted {
          public:
            std::unique_ptr<WorkerThreadTask> task;
            WorkerThreadTaskManager* taskManager;
            std::shared_ptr<dawn_platform::WaitableEvent> waitableEvent;
        };

        static void DoWaitableTask(void* task);

        std::mutex mPendingTasksMutex;
        std::unordered_map<WorkerThreadTask*, Ref<WaitableTask>> mPendingTasks;
        dawn_platform::WorkerTaskPool* mWorkerTaskPool;
    };

}  // namespace dawn_native

#endif
