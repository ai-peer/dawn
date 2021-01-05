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
//
// WorkerThreadTests:
//     Simple tests for the worker thread class.

#include <gtest/gtest.h>

#include <list>
#include <memory>
#include <mutex>
#include <queue>

#include "common/WorkerThread.h"
#include "utils/SystemUtils.h"

namespace {
    struct SimpleTaskResult {
        uint32_t id;
        bool isDone = false;
    };

    // A thread-safe queue that stores the task results.
    class ConcurrentTaskResultQueue {
      public:
        void TaskCompleted(const SimpleTaskResult& result) {
            ASSERT_TRUE(result.isDone);

            std::lock_guard<std::mutex> waitableLock(mMutex);
            mTaskResultQueue.push(result);
        }

        std::vector<SimpleTaskResult> GetAndPopCompletedTasks() {
            std::lock_guard<std::mutex> waitableLock(mMutex);

            std::vector<SimpleTaskResult> results;
            while (!mTaskResultQueue.empty()) {
                results.push_back(mTaskResultQueue.front());
                mTaskResultQueue.pop();
            }
            return results;
        }

      private:
        std::mutex mMutex;
        std::queue<SimpleTaskResult> mTaskResultQueue;
    };

    // A simple task
    class SimpleTask : public dawn_native::Closure {
      public:
        SimpleTask(uint32_t id, ConcurrentTaskResultQueue* resultQueue)
            : mId(id), mResultQueue(resultQueue) {
        }
        void operator()() override {
            SimpleTaskResult result;
            result.id = mId;
            result.isDone = true;
            mResultQueue->TaskCompleted(result);
        }

        uint32_t GetID() const {
            return mId;
        }

      private:
        uint32_t mId;
        ConcurrentTaskResultQueue* mResultQueue;
    };

    // A simple implementation of task tracker which is only called in main thread and not
    // thread-safe.
    class Tracker : public dawn_native::NonCopyable {
      public:
        Tracker() : mTaskId(0) {
        }

        std::shared_ptr<SimpleTask> CreateSimpleTask() {
            return std::make_shared<SimpleTask>(++mTaskId, &mCompletedTaskResultQueue);
        }

        void StartNewTask(std::shared_ptr<SimpleTask> simpleTask,
                          std::shared_ptr<dawn_native::WorkerThreadPool> pool) {
            mTasksInFlight.push_back(pool->PostWorkerTask(simpleTask));
        }

        uint64_t GetTasksInFlightCount() {
            return mTasksInFlight.size();
        }

        void WaitAll() {
            for (auto iter = mTasksInFlight.begin(); iter != mTasksInFlight.end(); ++iter) {
                (*iter)->Wait();
            }
        }

        // In Tick() we clean up all the completed tasks and consume all the available results.
        void Tick() {
            auto iter = mTasksInFlight.begin();
            while (iter != mTasksInFlight.end()) {
                if ((*iter)->IsReady()) {
                    iter = mTasksInFlight.erase(iter);
                } else {
                    ++iter;
                }
            }

            const std::vector<SimpleTaskResult>& results =
                mCompletedTaskResultQueue.GetAndPopCompletedTasks();
            for (const SimpleTaskResult& result : results) {
                EXPECT_TRUE(result.isDone);
            }
        }

      private:
        std::atomic_uint32_t mTaskId;

        std::list<std::shared_ptr<dawn_native::WaitableEvent>> mTasksInFlight;
        ConcurrentTaskResultQueue mCompletedTaskResultQueue;
    };

}  // anonymous namespace

class WorkerThreadTest : public testing::Test {};

// Emulate the basic usage of worker thread pool in CreateReady*Pipeline().
TEST_F(WorkerThreadTest, Basic) {
    std::shared_ptr<dawn_native::WorkerThreadPool> pool = dawn_native::WorkerThreadPool::Create();
    Tracker tracker;

    constexpr uint32_t kTaskCount = 4;
    std::vector<std::shared_ptr<SimpleTask>> tasks;
    for (uint32_t i = 0; i < kTaskCount; ++i) {
        tasks.push_back(tracker.CreateSimpleTask());
    }

    for (uint32_t i = 0; i < kTaskCount; ++i) {
        tracker.StartNewTask(tasks[i], pool);
    }
    EXPECT_EQ(kTaskCount, tracker.GetTasksInFlightCount());

    // Let all the tasks run for a while.
    utils::USleep(1000);

    // Wait for the completion of all the tasks.
    tracker.WaitAll();
    EXPECT_EQ(0u, pool->GetRunningThreadsCount());

    tracker.Tick();
    EXPECT_EQ(0u, tracker.GetTasksInFlightCount());
}
