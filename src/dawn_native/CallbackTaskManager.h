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

#include "dawn_native/IntegerTypes.h"

namespace dawn_platform {
    class WaitableEvent;
    class WorkerTaskPool;
}  // namespace dawn_platform

namespace dawn_native {
    class DeviceBase;

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

    class WaitableEventManager;

    class WorkerThreadTask {
      public:
        explicit WorkerThreadTask(DeviceBase* device);
        virtual ~WorkerThreadTask();
        static void DoTask(void* userdata);

        void Start();

      protected:
        DeviceBase* mDevice;

      private:
        virtual void Run() = 0;

        dawn_platform::WaitableEvent* mWaitableEvent;
    };

    class WaitableEventManager {
      public:
        ~WaitableEventManager();

        void TrackWaitableEvent(std::unique_ptr<dawn_platform::WaitableEvent> waitableEvent);
        bool HasWaitableEventsInFlight() const;

        bool HasCompletedWaitableEvent();
        void AddCompletedWaitableEvent(dawn_platform::WaitableEvent* waitableEvent);
        void ClearAllCompletedWaitableEvents();
        void WaitAndClearAllWaitableEvent();

      private:
        std::unordered_map<dawn_platform::WaitableEvent*,
                           std::unique_ptr<dawn_platform::WaitableEvent>>
            mWaitableEventsInFlight;

        std::mutex mCompletedWaitableEventQueueMutex;
        std::vector<dawn_platform::WaitableEvent*> mCompletedWaitableEventQueue;
    };

}  // namespace dawn_native

#endif
