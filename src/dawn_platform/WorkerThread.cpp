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

#include "dawn_platform/WorkerThread.h"

#include <functional>
#include <mutex>
#include <thread>

#include "common/Assert.h"

namespace {

    class AsyncWaitableEvent final : public dawn_platform::WaitableEvent {
      public:
        explicit AsyncWaitableEvent(std::function<void()> asyncFunction) : mIsComplete(false) {
            std::thread workerThread(asyncFunction);
            workerThread.detach();
        }
        void Wait() override {
            std::unique_lock<std::mutex> lock(mMutex);
            mCondition.wait(lock, [this] { return mIsComplete; });
        }
        bool IsComplete() override {
            std::lock_guard<std::mutex> lock(mMutex);
            return mIsComplete;
        }
        void MarkAsComplete() override {
            std::lock_guard<std::mutex> lock(mMutex);
            mIsComplete = true;
            mCondition.notify_all();
        }

      private:
        // To protect the concurrent accesses from both main thread and background
        // threads to the member fields.
        std::mutex mMutex;

        bool mIsComplete;
        std::condition_variable mCondition;
    };

}  // anonymous namespace

std::unique_ptr<dawn_platform::WaitableEvent> AsyncWorkerThreadPool::PostWorkerTask(
    dawn_platform::PostWorkerTaskCallback callback,
    void* userdata) {
    std::function<void()> doTask = [callback, userdata]() { callback(userdata); };
    return std::make_unique<AsyncWaitableEvent>(doTask);
}