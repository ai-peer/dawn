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

#include "dawn_native/WorkerThread.h"

#include <future>

#include "common/Assert.h"

namespace {

    class AsyncWaitableEvent final : public dawn_platform::WaitableEvent {
      public:
        AsyncWaitableEvent(std::function<void()> callback,
                           std::shared_ptr<dawn_platform::WorkerTaskPool> pool)
            : mCallback(callback), mPool(pool) {
            mFuture = std::async(std::launch::async, [this] {
                mCallback();
                mPool->TaskFinished();
            });
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
        std::function<void()> mCallback;
        std::shared_ptr<dawn_platform::WorkerTaskPool> mPool;
        std::future<void> mFuture;
    };

}  // anonymous namespace

WorkerTaskPoolWrapper::WorkerTaskPoolWrapper(dawn_platform::Platform* platform) {
    // TODO(jiawei.shao@intel.com): support DelegatedWorkerPool when
    // platform->CreateWorkerTaskPool() is not nullptr (for example when in Chromium we will use
    // the multi-threading infrastructure provided by Chromium.
    // TODO(jiawei.shao@intel.com): support FakeWorkerPool for UWP as threads cannot be created in
    // UWP appearantly.
    ASSERT(platform->CreateWorkerTaskPool() == nullptr);
    mPool = std::make_shared<dawn_platform::WorkerTaskPool>();
}

std::shared_ptr<dawn_platform::WaitableEvent> WorkerTaskPoolWrapper::PostWorkerTask(
    std::function<void()> callback) {
    return std::make_shared<AsyncWaitableEvent>(callback, mPool);
}
