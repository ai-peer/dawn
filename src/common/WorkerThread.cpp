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

namespace dawn_native {

    class AsyncWaitableEvent final : public WaitableEvent {
      public:
        AsyncWaitableEvent() {
        }
        ~AsyncWaitableEvent() override = default;

        void Wait() override;
        bool IsReady() override;

      private:
        friend class AsyncWorkerPool;
        void SetFuture(std::future<void>&& future);

        std::mutex mMutex;
        std::future<void> mFuture;
    };

    void AsyncWaitableEvent::SetFuture(std::future<void>&& future) {
        mFuture = std::move(future);
    }

    void AsyncWaitableEvent::Wait() {
        ASSERT(mFuture.valid());
        mFuture.wait();
    }

    bool AsyncWaitableEvent::IsReady() {
        std::lock_guard<std::mutex> lock(mMutex);

        ASSERT(mFuture.valid());
        return mFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    class AsyncWorkerPool final : public WorkerThreadPool {
      public:
        AsyncWorkerPool() = default;
        ~AsyncWorkerPool() override = default;

        std::shared_ptr<WaitableEvent> PostWorkerTask(std::shared_ptr<Closure> task) override;
    };

    // AsyncWorkerPool implementation
    std::shared_ptr<WaitableEvent> AsyncWorkerPool::PostWorkerTask(std::shared_ptr<Closure> task) {
        auto waitable = std::make_shared<AsyncWaitableEvent>();

        auto future = std::async(std::launch::async, [task, this] {
            (*task)();

            ASSERT(mRunningThreads != 0);
            --mRunningThreads;
        });

        ++mRunningThreads;
        {
            std::lock_guard<std::mutex> waitableLock(waitable->mMutex);
            waitable->SetFuture(std::move(future));
        }

        return std::move(waitable);
    }

    // static
    std::shared_ptr<WorkerThreadPool> WorkerThreadPool::Create() {
        std::shared_ptr<WorkerThreadPool> pool;

        // TODO(jiawei.shao@intel.com): support creating DelegatedThreadPool which will use
        // DawnPlatform::PostWorkerTask() provided by the external caller.
        pool.reset(new AsyncWorkerPool());
        return pool;
    }

    uint64_t WorkerThreadPool::GetRunningThreadsCount() const {
        return mRunningThreads;
    }

}  // namespace dawn_native
