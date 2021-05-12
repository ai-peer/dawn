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

#include <atomic>
#include <thread>

#include "common/Assert.h"

namespace {

    class AsyncWaitableEvent final : public dawn_platform::WaitableEvent {
      public:
        AsyncWaitableEvent(dawn_platform::PostWorkerTaskCallback callback, void* userdata)
            : mThread([this, callback, userdata]() {
                  callback(userdata);
                  mRunning = false;
              }) {
        }

        void Wait() override {
            mThread.join();
        }

        bool IsComplete() override {
            return mRunning == false;
        }

      private:
        std::atomic<bool> mRunning{true};
        std::thread mThread;
    };

}  // anonymous namespace

std::unique_ptr<dawn_platform::WaitableEvent> AsyncWorkerThreadPool::PostWorkerTask(
    dawn_platform::PostWorkerTaskCallback callback,
    void* userdata) {
    return std::make_unique<AsyncWaitableEvent>(callback, userdata);
}