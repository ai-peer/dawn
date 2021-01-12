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

// WorkerThread:
//   Asychronous tasks/threads for Dawn, similar to a WorkerThread in ANGLE and a TaskRunner in
//   Chromium.
//

#include "common/WorkerThread.h"

#include "Assert.h"

#include <future>

namespace {

    class AsyncWaitableEvent final : public dawn_platform::WaitableEvent {
      public:
        AsyncWaitableEvent(std::function<void()> callback,
                           std::shared_ptr<dawn_platform::WorkerTaskPool> pool)
            : mPool(pool) {
            mFuture = std::async(std::launch::async, [callback, this] {
                // The implementation of PostWorkerTask in DawnPlatform.cpp is sync, so we can do
                // the following things.
                mPool->PostWorkerTask(
                    [callback](void* userdata) {
                        callback();
                        static_cast<AsyncWaitableEvent*>(userdata)->Signal();
                    },
                    this);
            });
        }

        void Signal() override {
            mPool->TaskFinished();
        }

        void Wait() override {
            ASSERT(mFuture.valid());
            mFuture.wait();
        }
        bool IsComplete() override {
            ASSERT(mFuture.valid());
            return mFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }

      private:
        std::shared_ptr<dawn_platform::WorkerTaskPool> mPool;
        std::future<void> mFuture;
    };

}  // anonymous namespace

WorkerTaskPoolAgent::WorkerTaskPoolAgent(dawn_platform::Platform* platform) {
    // TODO(jiawei.shao@intel.com): support DelegatedWorkerPool when
    // platform->CreateWorkerTaskPool() is not nullptr (for example when in Chromium we will use
    // the multi-threading infrastructure provided by Chromium.
    // TODO(jiawei.shao@intel.com): support FakeWorkerPool for UWP as threads cannot be created in
    // UWP appearantly.
    ASSERT(platform->CreateWorkerTaskPool() == nullptr);
    mPool = std::make_shared<dawn_platform::WorkerTaskPool>();
}

std::shared_ptr<dawn_platform::WaitableEvent> WorkerTaskPoolAgent::PostWorkerTask(
    std::function<void()> callback) {
    return std::make_shared<AsyncWaitableEvent>(callback, mPool);
}
