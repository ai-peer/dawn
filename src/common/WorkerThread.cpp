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

    class AsyncWorkerPool final : public WorkerTaskPool {
      public:
        AsyncWorkerPool() = default;

        std::shared_ptr<WaitableEvent> PostWorkerTask(std::shared_ptr<Closure> task) override;
        void TaskFinished();
    };

    class AsyncWaitableEvent final : public WaitableEvent {
      public:
        AsyncWaitableEvent(std::shared_ptr<Closure> task, AsyncWorkerPool* pool) {
            mFuture = std::async(std::launch::async, [task, pool] {
                (*task)();

                pool->TaskFinished();
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
        std::future<void> mFuture;
    };

    std::shared_ptr<WaitableEvent> AsyncWorkerPool::PostWorkerTask(std::shared_ptr<Closure> task) {
        ++mRunningTasks;
        return std::make_shared<AsyncWaitableEvent>(task, this);
    }

    void AsyncWorkerPool::TaskFinished() {
        ASSERT(mRunningTasks != 0);
        --mRunningTasks;
    }

}  // anonymous namespace

WorkerTaskPool::WorkerTaskPool() : mRunningTasks(0) {
}

// static
std::shared_ptr<WorkerTaskPool> WorkerTaskPool::Create() {
    std::shared_ptr<WorkerTaskPool> pool;

    // TODO(jiawei.shao@intel.com): support creating DelegatedWorkerPool which will use
    // DawnPlatform::PostWorkerTask() provided by the external caller.
    // TODO(jiawei.shao@intel.com): support FakeWorkerPool for UWP as threads cannot be created in
    // UWP appearantly.
    pool.reset(new AsyncWorkerPool());
    return pool;
}

uint64_t WorkerTaskPool::GetRunningTasksCount() const {
    return mRunningTasks;
}
