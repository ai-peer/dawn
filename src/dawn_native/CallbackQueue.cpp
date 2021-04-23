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

#include "dawn_native/CallbackQueue.h"

#include "dawn_native/Device.h"

namespace dawn_native {

    CallbackTaskInFlight::~CallbackTaskInFlight() = default;

    void CallbackTaskInFlight::HandleShutDown() {
    }

    CallbackQueue::CallbackQueue(DeviceBase* device) : mDevice(device) {
    }

    CallbackQueue::~CallbackQueue() {
        ASSERT(mTasksInFlight.Empty());
    }

    void CallbackQueue::AddCallback(std::unique_ptr<CallbackTaskInFlight> task,
                                    ExecutionSerial serial) {
        mTasksInFlight.Enqueue(std::move(task), serial);
        mDevice->AddFutureSerial(serial);
    }

    std::vector<std::unique_ptr<CallbackTaskInFlight>>
    CallbackQueue::AcquireCallbacksWithFinishedSerial(ExecutionSerial finishedSerial) {
        std::vector<std::unique_ptr<CallbackTaskInFlight>> outputTasks;
        for (auto& task : mTasksInFlight.IterateUpTo(finishedSerial)) {
            outputTasks.push_back(std::move(task));
        }
        mTasksInFlight.ClearUpTo(finishedSerial);

        return outputTasks;
    }

    std::vector<std::unique_ptr<CallbackTaskInFlight>> CallbackQueue::AcquireAllCallbacks() {
        std::vector<std::unique_ptr<CallbackTaskInFlight>> outputTasks;
        for (auto& task : mTasksInFlight.IterateAll()) {
            outputTasks.push_back(std::move(task));
        }
        mTasksInFlight.Clear();

        return outputTasks;
    }

    bool CallbackQueue::Empty() const {
        return mTasksInFlight.Empty();
    }

}  // namespace dawn_native
