// Copyright 2023 The Dawn Authors
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

#include <atomic>
#include <vector>

#include "dawn/tests/DawnTest.h"

namespace dawn {
namespace {

enum class WaitMode {
    // FIXME: more modes
    TimedWaitAny,
    SpinWaitAny,
    SpinProcessEvents,
};

std::ostream& operator<<(std::ostream& o, WaitMode waitMode) {
    switch (waitMode) {
        case WaitMode::TimedWaitAny:
            return o << "TimedWaitAny";
        case WaitMode::SpinWaitAny:
            return o << "SpinWaitAny";
        case WaitMode::SpinProcessEvents:
            return o << "SpinProcessEvents";
    }
}

DAWN_TEST_PARAM_STRUCT(EventTestParams, WaitMode);

class EventTests : public DawnTestWithParams<EventTestParams> {
  protected:
    std::vector<wgpu::FutureWaitInfo> mFutures;
    std::atomic<uint64_t> mCallbackCompletionCount;

    void SetUp() override {
        DawnTestWithParams::SetUp();
        DAWN_TEST_UNSUPPORTED_IF(GetParam().mWaitMode == WaitMode::TimedWaitAny && UsesWire());
    }

    void TrivialSubmit() {
        wgpu::CommandBuffer cb = device.CreateCommandEncoder().Finish();
        queue.Submit(1, &cb);
    }

    wgpu::CallbackMode GetCallbackMode() {
        switch (GetParam().mWaitMode) {
            case WaitMode::TimedWaitAny:
            case WaitMode::SpinWaitAny:
                return wgpu::CallbackMode::Future;
            case WaitMode::SpinProcessEvents:
                return wgpu::CallbackMode::ProcessEvents;
        }
    }

    void TrackForTest(wgpu::Future future) {
        mFutures.push_back(wgpu::FutureWaitInfo{future, false});
    }

    wgpu::Future OnSubmittedWorkDone(WGPUQueueWorkDoneStatus expectedStatus) {
        struct Userdata {
            EventTests* self;
            WGPUQueueWorkDoneStatus expectedStatus;
        };
        Userdata* userdata = new Userdata{this, expectedStatus};

        return queue.OnSubmittedWorkDoneF({
            GetCallbackMode(),
            [](WGPUQueueWorkDoneStatus status, void* userdata) {
                Userdata* u = reinterpret_cast<Userdata*>(userdata);
                u->self->mCallbackCompletionCount++;
                ASSERT_EQ(status, u->expectedStatus);
                delete u;
            },
            userdata,
        });
    }

    void TestWaitAll(bool loopOnlyOnce = false) {
        const auto start = std::chrono::high_resolution_clock::now();
        auto testTimeExceeded = [=]() -> bool {
            return std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(5);
        };

        switch (GetParam().mWaitMode) {
            case WaitMode::TimedWaitAny: {
                bool emptyWait = mFutures.size() == 0;
                // Loop at least once so we can test it with 0 futures.
                do {
                    ASSERT_FALSE(testTimeExceeded());
                    wgpu::WaitStatus status;

                    uint64_t oldCompletionCount = mCallbackCompletionCount;
                    // Any futures should succeed within a few milliseconds at most.
                    status = GetInstance().WaitAny(mFutures.size(), mFutures.data(), UINT64_MAX);
                    ASSERT_EQ(status, wgpu::WaitStatus::Success);
                    ASSERT_TRUE(emptyWait || mCallbackCompletionCount > oldCompletionCount);

                    // This should succeed instantly because some futures completed already.
                    status = GetInstance().WaitAny(mFutures.size(), mFutures.data(), 0);
                    ASSERT_EQ(status, wgpu::WaitStatus::Success);

                    if (!emptyWait) {
                        size_t oldSize = mFutures.size();
                        mFutures.erase(std::remove_if(mFutures.begin(), mFutures.end(),
                                                      [](const wgpu::FutureWaitInfo& info) {
                                                          return info.completed;
                                                      }),
                                       mFutures.end());
                        ASSERT_LT(mFutures.size(), oldSize);
                    }
                } while (mFutures.size() > 0 && !loopOnlyOnce);
            } break;
            case WaitMode::SpinWaitAny: {
                bool emptyWait = mFutures.size() == 0;
                // Loop at least once so we can test it with 0 futures.
                do {
                    ASSERT_FALSE(testTimeExceeded());

                    uint64_t oldCompletionCount = mCallbackCompletionCount;
                    FlushWire();
                    device.Tick();
                    auto status = GetInstance().WaitAny(mFutures.size(), mFutures.data(), 0);
                    if (status == wgpu::WaitStatus::TimedOut) {
                        continue;
                    }
                    ASSERT_TRUE(status == wgpu::WaitStatus::Success);
                    ASSERT_TRUE(emptyWait || mCallbackCompletionCount > oldCompletionCount);

                    if (!emptyWait) {
                        size_t oldSize = mFutures.size();
                        mFutures.erase(std::remove_if(mFutures.begin(), mFutures.end(),
                                                      [](const wgpu::FutureWaitInfo& info) {
                                                          return info.completed;
                                                      }),
                                       mFutures.end());
                        ASSERT_LT(mFutures.size(), oldSize);
                    }
                } while (mFutures.size() > 0 && !loopOnlyOnce);
            } break;
            case WaitMode::SpinProcessEvents: {
                uint64_t oldCompletionCount = mCallbackCompletionCount;
                size_t futuresCount = mFutures.size();
                do {
                    ASSERT_FALSE(testTimeExceeded());

                    FlushWire();
                    device.Tick();
                    GetInstance().ProcessEvents();
                } while (
                    // mFutures will be full of null futures, but its size still tells us how many
                    // we're waiting for.
                    mCallbackCompletionCount - oldCompletionCount < futuresCount && !loopOnlyOnce);
                mFutures.clear();

                // Ensure we completed exactly what we expected:
                // - We didn't complete something that wasn't supposed to complete.
                // - If loopOnlyOnce=true, we completed fully in one loop.
                ASSERT_EQ(mCallbackCompletionCount - oldCompletionCount, futuresCount);
            } break;
        }
    }
};

// Wait when no events have been requested.
TEST_P(EventTests, NoEvents) {
    TestWaitAll();
}

// WorkDone event after submitting some trivial work.
TEST_P(EventTests, WorkDone_Simple) {
    TrivialSubmit();
    TrackForTest(OnSubmittedWorkDone(WGPUQueueWorkDoneStatus_Success));
    TestWaitAll();
}

// WorkDone event before device loss, wait afterward.
TEST_P(EventTests, WorkDone_AcrossDeviceLoss) {
    TrivialSubmit();
    TrackForTest(OnSubmittedWorkDone(WGPUQueueWorkDoneStatus_Success));
    LoseDeviceForTesting();
    TestWaitAll();
}

// WorkDone event after device loss.
TEST_P(EventTests, WorkDone_AfterDeviceLoss) {
    TrivialSubmit();
    LoseDeviceForTesting();
    ASSERT_DEVICE_ERROR(TrackForTest(OnSubmittedWorkDone(WGPUQueueWorkDoneStatus_Success)));
    TestWaitAll();
}

// WorkDone event twice after submitting some trivial work.
TEST_P(EventTests, WorkDone_Twice) {
    TrivialSubmit();
    TrackForTest(OnSubmittedWorkDone(WGPUQueueWorkDoneStatus_Success));
    TrackForTest(OnSubmittedWorkDone(WGPUQueueWorkDoneStatus_Success));
    TestWaitAll();
}

// WorkDone event without ever having submitted any work.
TEST_P(EventTests, WorkDone_NoWork) {
    TrackForTest(OnSubmittedWorkDone(WGPUQueueWorkDoneStatus_Success));
    TestWaitAll();
    TrackForTest(OnSubmittedWorkDone(WGPUQueueWorkDoneStatus_Success));
    TrackForTest(OnSubmittedWorkDone(WGPUQueueWorkDoneStatus_Success));
    TestWaitAll();
}

// WorkDone event after all work has completed already.
TEST_P(EventTests, WorkDone_AlreadyCompleted) {
    TrivialSubmit();
    TrackForTest(OnSubmittedWorkDone(WGPUQueueWorkDoneStatus_Success));
    TestWaitAll();
    TrackForTest(OnSubmittedWorkDone(WGPUQueueWorkDoneStatus_Success));
    TestWaitAll();
}

// WorkDone events waited in reverse order.
// FIXME: Flaky without wire
TEST_P(EventTests, WorkDone_OutOfOrder) {
    // This test was flaky before a bugfix, so run it many times to catch flakes.
    for (int i = 0; i < 100; ++i) {
        TrivialSubmit();
        wgpu::Future f1 = OnSubmittedWorkDone(WGPUQueueWorkDoneStatus_Success);
        TrivialSubmit();
        wgpu::Future f2 = OnSubmittedWorkDone(WGPUQueueWorkDoneStatus_Success);

        // Note the callback ordering guarantees don't apply because f1 doesn't have
        // an opportunity to complete yet.
        TrackForTest(f2);
        TestWaitAll();
        TrackForTest(f1);
        TestWaitAll(true);  // FIXME: fails with wire due to this true
    }
}

//

DAWN_INSTANTIATE_TEST_P(
    EventTests,
    // TODO(crbug.com/dawn/1987): Enable tests for the rest of the backends
    // TODO(crbug.com/dawn/1987): Enable tests on the wire (though they'll behave differently)
    {D3D12Backend(), MetalBackend()},
    {
        WaitMode::TimedWaitAny,
        WaitMode::SpinWaitAny,
        WaitMode::SpinProcessEvents,
    });

}  // anonymous namespace
}  // namespace dawn
