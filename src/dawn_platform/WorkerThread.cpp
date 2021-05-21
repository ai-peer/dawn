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
        AsyncWaitableEvent() : waitableEventImpl(std::make_shared<WaitableEventImpl>()) {
        }
        void Wait() override {
            std::unique_lock<std::mutex> lock(waitableEventImpl->mutex);
            waitableEventImpl->condition.wait(lock,
                                              [this] { return waitableEventImpl->isComplete; });
        }
        bool IsComplete() override {
            std::lock_guard<std::mutex> lock(waitableEventImpl->mutex);
            return waitableEventImpl->isComplete;
        }

        struct WaitableEventImpl {
            WaitableEventImpl() : isComplete(false) {
            }
            // To protect the concurrent accesses from both main thread and background
            // threads to the member fields.

            void MarkAsComplete() {
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    isComplete = true;
                }
                condition.notify_all();
            }

            std::mutex mutex;
            std::condition_variable condition;
            bool isComplete;
        };

        std::shared_ptr<WaitableEventImpl> waitableEventImpl;
    };

}  // anonymous namespace

namespace dawn_platform {

    std::unique_ptr<dawn_platform::WaitableEvent> AsyncWorkerThreadPool::PostWorkerTask(
        dawn_platform::PostWorkerTaskCallback callback,
        void* userdata) {
        std::unique_ptr<AsyncWaitableEvent> waitableEvent = std::make_unique<AsyncWaitableEvent>();
        std::function<void()> doTask = [callback, userdata,
                                        waitableEventPtr = waitableEvent.get()]() {
            // As WaitableEvent is stored in userdata and will always be released in callback(), we
            // need to reference userdata here to ensure waitableEventImpl always points to a valid
            // object.
            std::shared_ptr<AsyncWaitableEvent::WaitableEventImpl> waitableEventImpl =
                waitableEventPtr->waitableEventImpl;
            callback(userdata);
            waitableEventImpl->MarkAsComplete();
        };

        std::thread thread(doTask);
        thread.detach();

        return waitableEvent;
    }

}  // namespace dawn_platform
