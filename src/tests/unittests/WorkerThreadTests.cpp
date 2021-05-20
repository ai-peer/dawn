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

#include <memory>
#include <mutex>

#include "common/NonCopyable.h"
#include "dawn_platform/DawnPlatform.h"

namespace {

    struct SimpleTaskResult {
        uint32_t id;
        bool isDone = false;
    };

    class SimpleTask;

    class Tracker : public NonCopyable {
      public:
        explicit Tracker(dawn_platform::WorkerTaskPool* pool);

        void PostWorkerTask(SimpleTask* simpleTask);
        void TaskCompleted(SimpleTaskResult taskResult);
        size_t GetTasksOnFlightCount() const;
        void WaitAll();

        std::vector<SimpleTaskResult> GetCompletedTaskResults();

      private:
        std::mutex mMutex;
        std::condition_variable mConditionVariable;

        size_t mTotalTasksOnFlight;
        std::vector<SimpleTaskResult> mTaskResults;

        dawn_platform::WorkerTaskPool* mPool;
    };

    // A simple task that will be executed asynchronously with pool->PostWorkerTask().
    class SimpleTask : public NonCopyable {
      public:
        // SimpleTask is always created on heap and released in DoTaskOnWorkerTaskPool().
        static SimpleTask* Create(uint32_t id, Tracker* tracker) {
            return new SimpleTask(id, tracker);
        }

        void StartWorkerThreadTask(Tracker* tracker) {
            tracker->PostWorkerTask(this);
        }

      private:
        friend class Tracker;

        SimpleTask(uint32_t id, Tracker* tracker) : mId(id), mTracker(tracker) {
        }

        static void DoTaskOnWorkerTaskPool(void* task) {
            std::unique_ptr<SimpleTask> simpleTaskPtr(static_cast<SimpleTask*>(task));
            simpleTaskPtr->doTask();
        }

        void doTask() {
            SimpleTaskResult result;
            result.id = mId;
            result.isDone = true;
            mTracker->TaskCompleted(result);
        }

        uint32_t mId;
        Tracker* mTracker;
    };

    Tracker::Tracker(dawn_platform::WorkerTaskPool* pool) : mTotalTasksOnFlight(0), mPool(pool) {
    }

    void Tracker::PostWorkerTask(SimpleTask* simpleTask) {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            ++mTotalTasksOnFlight;
        }

        mPool->PostWorkerTask(SimpleTask::DoTaskOnWorkerTaskPool, simpleTask);
    }

    void Tracker::TaskCompleted(SimpleTaskResult result) {
        std::lock_guard<std::mutex> lock(mMutex);
        mTaskResults.push_back(result);
        --mTotalTasksOnFlight;
        mConditionVariable.notify_all();
    }

    size_t Tracker::GetTasksOnFlightCount() const {
        return mTotalTasksOnFlight;
    }

    void Tracker::WaitAll() {
        std::unique_lock<std::mutex> lock(mMutex);
        mConditionVariable.wait(lock, [this] { return mTotalTasksOnFlight == 0; });
    }

    std::vector<SimpleTaskResult> Tracker::GetCompletedTaskResults() {
        std::vector<SimpleTaskResult> completedTaskResults;
        {
            std::unique_lock<std::mutex> lock(mMutex);
            completedTaskResults.swap(mTaskResults);
        }
        return completedTaskResults;
    }

}  // anonymous namespace

class WorkerThreadTest : public testing::Test {};

// Emulate the basic usage of worker thread pool in Create*PipelineAsync().
TEST_F(WorkerThreadTest, Basic) {
    dawn_platform::Platform platform;
    std::unique_ptr<dawn_platform::WorkerTaskPool> pool = platform.CreateWorkerTaskPool();
    Tracker tracker(pool.get());

    constexpr size_t kTaskCount = 4;
    std::set<size_t> allTaskIDs;
    for (uint32_t i = 0; i < kTaskCount; ++i) {
        tracker.PostWorkerTask(SimpleTask::Create(i, &tracker));
        allTaskIDs.insert(i);
    }

    // Wait for the completion of all the tasks.
    tracker.WaitAll();

    std::vector<SimpleTaskResult> completedTaskResults = tracker.GetCompletedTaskResults();
    EXPECT_EQ(kTaskCount, completedTaskResults.size());
    for (SimpleTaskResult result : completedTaskResults) {
        ASSERT_TRUE(result.isDone);

        const auto& iter = allTaskIDs.find(result.id);
        ASSERT_NE(allTaskIDs.cend(), iter);
        allTaskIDs.erase(iter);
    }
    EXPECT_TRUE(allTaskIDs.empty());

    EXPECT_EQ(0u, tracker.GetTasksOnFlightCount());
}
