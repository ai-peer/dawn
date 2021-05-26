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

#ifndef DAWNNATIVE_ASYC_TASK_H_
#define DAWNNATIVE_ASYC_TASK_H_

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "common/RefCounted.h"

namespace dawn_platform {
    class WaitableEvent;
    class WorkerTaskPool;
}  // namespace dawn_platform

namespace dawn_native {

    using AsyncTask = std::function<void()>;

    class AsnycTaskManager {
      public:
        explicit AsnycTaskManager(dawn_platform::WorkerTaskPool* workerTaskPool);

        void PostTask(AsyncTask asyncTask);
        void WaitAllPendingTasks();

      private:
        class WaitableTask : public RefCounted {
          public:
            AsyncTask asyncTask;
            AsnycTaskManager* taskManager;
            std::unique_ptr<dawn_platform::WaitableEvent> waitableEvent;
        };

        static void DoWaitableTask(void* task);
        void MakeTaskCompleted(WaitableTask* task);

        std::mutex mPendingTasksMutex;
        std::unordered_map<WaitableTask*, Ref<WaitableTask>> mPendingTasks;
        dawn_platform::WorkerTaskPool* mWorkerTaskPool;
    };

}  // namespace dawn_native

#endif
