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

#ifndef COMMON_WORKERTHREAD_H_
#define COMMON_WORKERTHREAD_H_

// WorkerThread:
//   Asychronous tasks/threads for Dawn, similar to a WorkerThread in ANGLE and a TaskRunner in
//   Chromium.
//

#include <atomic>
#include <memory>

namespace dawn_native {

    class WorkerThreadPool;

    class NonCopyable {
      protected:
        constexpr NonCopyable() = default;
        ~NonCopyable() = default;

      private:
        NonCopyable(const NonCopyable&) = delete;
        void operator=(const NonCopyable&) = delete;
    };

    // A callback function with no return value and no arguments.
    class Closure : public NonCopyable {
      public:
        virtual ~Closure() = default;
        virtual void operator()() = 0;
    };

    // An event that we can wait on.
    class WaitableEvent {
      public:
        WaitableEvent() = default;
        virtual ~WaitableEvent() = default;

        // Waits indefinitely for the event to be signaled.
        virtual void Wait() = 0;

        // Peeks whether the event is ready. If ready, wait() will not block.
        virtual bool IsReady() = 0;
        void SetWorkerThreadPool(std::shared_ptr<WorkerThreadPool> pool) {
            mPool = pool;
        }

      private:
        std::shared_ptr<WorkerThreadPool> mPool;
    };

    // Request WorkerThreads from the WorkerThreadPool. Each pool can keep worker threads around so
    // we avoid the costly spin up and spin down time.
    class WorkerThreadPool : public NonCopyable {
      public:
        WorkerThreadPool() : mRunningThreads(0) {
        }
        virtual ~WorkerThreadPool() = default;

        static std::shared_ptr<WorkerThreadPool> Create();

        // Returns an event to wait on for the task to finish.
        // If the pool fails to create the task, returns null.
        virtual std::shared_ptr<WaitableEvent> PostWorkerTask(std::shared_ptr<Closure> task) = 0;

        uint64_t GetRunningThreadsCount() const;

      protected:
        std::atomic<uint64_t> mRunningThreads;
    };

}  // namespace dawn_native

#endif
