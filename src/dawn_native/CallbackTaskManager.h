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

namespace dawn_platform {
    class WaitableEvent;
    class WorkerTaskPool;
}  // namespace dawn_platform

namespace dawn_native {
    class WaitableEventManager;
    class WorkerThreadTask;

    struct CallbackTask {
      public:
        CallbackTask();
        virtual ~CallbackTask() = default;
        void Finish();
        virtual void HandleShutDown() = 0;
        virtual void HandleDeviceLoss() = 0;

      protected:
        virtual void FinishImpl() = 0;

      private:
        friend class WorkerThreadTask;
        size_t mSerial;
        WaitableEventManager* mOptionalWaitableEventManager;
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
        explicit WorkerThreadTask(WaitableEventManager* waitableEventManager,
                                  CallbackTaskManager* callbackTaskManager);
        virtual ~WorkerThreadTask();
        void Start(dawn_platform::WorkerTaskPool* workerTaskPool);

      private:
        virtual std::unique_ptr<CallbackTask> Run() = 0;
        static void DoTask(void* userdata);

        friend class WaitableEventManager;
        WaitableEventManager* mWaitableEventManager;
        CallbackTaskManager* mCallbackTaskManager;
        size_t mSerial;
    };

    class WaitableEventManager {
      public:
        WaitableEventManager();
        ~WaitableEventManager();

        void StartWorkerThreadTask(WorkerThreadTask* task,
                                   dawn_platform::WorkerTaskPool* workerTaskPool);
        bool HasWaitableEventsInFlight() const;
        void RemoveCompletedWaitableEvent(size_t serial);

        void WaitAndClearAllWaitableEvent();

      private:
        size_t NextSerial();
        void TrackWaitableEvent(size_t serial,
                                std::unique_ptr<dawn_platform::WaitableEvent> waitableEvent);

        size_t mSerial;
        std::unordered_map<size_t, std::unique_ptr<dawn_platform::WaitableEvent>>
            mWaitableEventsInFlight;
    };

}  // namespace dawn_native

#endif
