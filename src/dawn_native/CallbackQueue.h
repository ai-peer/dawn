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

#ifndef DAWNNATIVE_CALLBACK_QUEUE_H_
#define DAWNNATIVE_CALLBACK_QUEUE_H_

#include <memory>

#include "common/SerialQueue.h"
#include "dawn_native/IntegerTypes.h"

namespace dawn_native {

    class DeviceBase;

    // The base struct for the tasks which have callbacks to call in DeviceBase::Tick().
    struct CallbackTaskInFlight {
        virtual ~CallbackTaskInFlight();
        virtual void Finish() = 0;
        virtual void HandleDeviceLoss() = 0;
        virtual void HandleShutDown();
    };

    // A queue that stores all the tasks which have callbacks to call in DeviceBase::Tick().
    // TODO(jiawei.shao@intel.com): make it thread safe.
    class CallbackQueue {
      public:
        explicit CallbackQueue(DeviceBase* device);
        ~CallbackQueue();

        void AddCallback(std::unique_ptr<CallbackTaskInFlight> task, ExecutionSerial serial);
        std::vector<std::unique_ptr<CallbackTaskInFlight>> AcquireCallbacksWithFinishedSerial(
            ExecutionSerial finishedSerial);
        std::vector<std::unique_ptr<CallbackTaskInFlight>> AcquireAllCallbacks();
        bool Empty() const;

      private:
        DeviceBase* mDevice;

        // TODO(jiawei.shao@intel.com): Protect mTasksInFlight with a mutex when we implement
        // the asynchronous version of Create*PipelineAsync().
        SerialQueue<ExecutionSerial, std::unique_ptr<CallbackTaskInFlight>> mTasksInFlight;
    };

}  // namespace dawn_native

#endif
