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

#ifndef SRC_DAWN_NATIVE_CALLBACKTASKMANAGER_H_
#define SRC_DAWN_NATIVE_CALLBACKTASKMANAGER_H_

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace dawn::native {

class CallbackTaskManager {
  public:
    CallbackTaskManager();
    ~CallbackTaskManager();

    void AddCallbackTask(std::function<void()> callbackTask);
    bool IsEmpty();
    std::vector<std::function<void()>> AcquireCallbackTasks();

  private:
    std::mutex mCallbackTaskQueueMutex;
    std::vector<std::function<void()>> mCallbackTaskQueue;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_CALLBACKTASKMANAGER_H_
