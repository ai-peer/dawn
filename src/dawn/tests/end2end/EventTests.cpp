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

enum class WaitType {
    TimedWaitAny,
    SpinWaitAny,
    SpinProcessEvents,
};

enum class WaitTypeAndCallbackMode {
    TimedWaitAny_Future,
    TimedWaitAny_FutureSpontaneous,
    SpinWaitAny_Future,
    SpinWaitAny_FutureSpontaneous,
    SpinProcessEvents_ProcessEvents,
    SpinProcessEvents_ProcessEventsSpontaneous,
};

std::ostream& operator<<(std::ostream& o, WaitTypeAndCallbackMode waitMode) {
    switch (waitMode) {
        case WaitTypeAndCallbackMode::TimedWaitAny_Future:
            return o << "TimedWaitAny_Future";
        case WaitTypeAndCallbackMode::SpinWaitAny_Future:
            return o << "SpinWaitAny_Future";
        case WaitTypeAndCallbackMode::SpinProcessEvents_ProcessEvents:
            return o << "SpinProcessEvents_ProcessEvents";
        case WaitTypeAndCallbackMode::TimedWaitAny_FutureSpontaneous:
            return o << "TimedWaitAny_FutureSpontaneous";
        case WaitTypeAndCallbackMode::SpinWaitAny_FutureSpontaneous:
            return o << "SpinWaitAny_FutureSpontaneous";
        case WaitTypeAndCallbackMode::SpinProcessEvents_ProcessEventsSpontaneous:
            return o << "SpinProcessEvents_ProcessEventsSpontaneous";
    }
}

DAWN_TEST_PARAM_STRUCT(EventTestParams, WaitTypeAndCallbackMode);

class EventTests : public DawnTestWithParams<EventTestParams> {
  protected:
    std::vector<wgpu::FutureWaitInfo> mFutures;
    std::atomic<uint64_t> mCallbacksCompletedCount = 0;
    uint64_t mCallbacksIssuedCount = 0;
    uint64_t mCallbacksWaitedCount = 0;

    void SetUp() override {
        DawnTestWithParams::SetUp();
        WaitTypeAndCallbackMode mode = GetParam().mWaitTypeAndCallbackMode;
        if (UsesWire()) {
            DAWN_TEST_UNSUPPORTED_IF(mode == WaitTypeAndCallbackMode::TimedWaitAny_Future ||
                                     mode ==
                                         WaitTypeAndCallbackMode::TimedWaitAny_FutureSpontaneous);
        }
    }

    void TrivialSubmit() {
        wgpu::CommandBuffer cb = device.CreateCommandEncoder().Finish();
        queue.Submit(1, &cb);
    }

    wgpu::CallbackMode GetCallbackMode() {
        switch (GetParam().mWaitTypeAndCallbackMode) {
            case WaitTypeAndCallbackMode::TimedWaitAny_Future:
            case WaitTypeAndCallbackMode::SpinWaitAny_Future:
                return wgpu::CallbackMode::Future;
            case WaitTypeAndCallbackMode::SpinProcessEvents_ProcessEvents:
                return wgpu::CallbackMode::ProcessEvents;
            case WaitTypeAndCallbackMode::TimedWaitAny_FutureSpontaneous:
            case WaitTypeAndCallbackMode::SpinWaitAny_FutureSpontaneous:
                return wgpu::CallbackMode::Future | wgpu::CallbackMode::Spontaneous;
            case WaitTypeAndCallbackMode::SpinProcessEvents_ProcessEventsSpontaneous:
                return wgpu::CallbackMode::ProcessEvents | wgpu::CallbackMode::Spontaneous;
        }
    }

    bool IsSpontaneous() { return GetCallbackMode() & wgpu::CallbackMode::Spontaneous; }

    void TrackForTest(wgpu::Future future) {
        mCallbacksIssuedCount++;

        switch (GetParam().mWaitTypeAndCallbackMode) {
            case WaitTypeAndCallbackMode::TimedWaitAny_Future:
            case WaitTypeAndCallbackMode::TimedWaitAny_FutureSpontaneous:
            case WaitTypeAndCallbackMode::SpinWaitAny_Future:
            case WaitTypeAndCallbackMode::SpinWaitAny_FutureSpontaneous:
                mFutures.push_back(wgpu::FutureWaitInfo{future, false});
                break;
            case WaitTypeAndCallbackMode::SpinProcessEvents_ProcessEvents:
            case WaitTypeAndCallbackMode::SpinProcessEvents_ProcessEventsSpontaneous:
                ASSERT_EQ(future.id, 0ull);
                break;
        }
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
                u->self->mCallbacksCompletedCount++;
                ASSERT_EQ(status, u->expectedStatus);
                delete u;
            },
            userdata,
        });
    }

    void TestWaitAll(bool loopOnlyOnce = false) {
        switch (GetParam().mWaitTypeAndCallbackMode) {
            case WaitTypeAndCallbackMode::TimedWaitAny_Future:
            case WaitTypeAndCallbackMode::TimedWaitAny_FutureSpontaneous:
                return TestWaitImpl(WaitType::TimedWaitAny);
            case WaitTypeAndCallbackMode::SpinWaitAny_Future:
            case WaitTypeAndCallbackMode::SpinWaitAny_FutureSpontaneous:
                return TestWaitImpl(WaitType::SpinWaitAny);
            case WaitTypeAndCallbackMode::SpinProcessEvents_ProcessEvents:
            case WaitTypeAndCallbackMode::SpinProcessEvents_ProcessEventsSpontaneous:
                return TestWaitImpl(WaitType::SpinProcessEvents);
        }
    }

    void TestWaitIncorrectly() {
        switch (GetParam().mWaitTypeAndCallbackMode) {
            case WaitTypeAndCallbackMode::TimedWaitAny_Future:
            case WaitTypeAndCallbackMode::TimedWaitAny_FutureSpontaneous:
            case WaitTypeAndCallbackMode::SpinWaitAny_Future:
            case WaitTypeAndCallbackMode::SpinWaitAny_FutureSpontaneous:
                return TestWaitImpl(WaitType::SpinProcessEvents);
            case WaitTypeAndCallbackMode::SpinProcessEvents_ProcessEvents:
            case WaitTypeAndCallbackMode::SpinProcessEvents_ProcessEventsSpontaneous:
                return TestWaitImpl(WaitType::SpinWaitAny);
        }
    }

  private:
    void TestWaitImpl(WaitType waitType, bool loopOnlyOnce = false) {
        uint64_t oldCompletedCount = mCallbacksCompletedCount;

        const auto start = std::chrono::high_resolution_clock::now();
        auto testTimeExceeded = [=]() -> bool {
            return std::chrono::high_resolution_clock::now() - start > std::chrono::seconds(5);
        };

        switch (waitType) {
            case WaitType::TimedWaitAny: {
                bool emptyWait = mFutures.size() == 0;
                // Loop at least once so we can test it with 0 futures.
                do {
                    ASSERT_FALSE(testTimeExceeded());
                    ASSERT(!UsesWire());
                    wgpu::WaitStatus status;

                    uint64_t oldCompletionCount = mCallbacksCompletedCount;
                    // Any futures should succeed within a few milliseconds at most.
                    status = GetInstance().WaitAny(mFutures.size(), mFutures.data(), UINT64_MAX);
                    ASSERT_EQ(status, wgpu::WaitStatus::Success);
                    bool mayHaveCompletedEarly = IsSpontaneous();
                    if (!mayHaveCompletedEarly && !emptyWait) {
                        ASSERT_GT(mCallbacksCompletedCount, oldCompletionCount);
                    }

                    // Verify this succeeds instantly because some futures completed already.
                    status = GetInstance().WaitAny(mFutures.size(), mFutures.data(), 0);
                    ASSERT_EQ(status, wgpu::WaitStatus::Success);

                    RemoveCompletedFutures();
                    if (loopOnlyOnce) {
                        break;
                    }
                } while (mFutures.size() > 0);
            } break;
            case WaitType::SpinWaitAny: {
                bool emptyWait = mFutures.size() == 0;
                // Loop at least once so we can test it with 0 futures.
                do {
                    ASSERT_FALSE(testTimeExceeded());

                    uint64_t oldCompletionCount = mCallbacksCompletedCount;
                    FlushWire();
                    device.Tick();
                    auto status = GetInstance().WaitAny(mFutures.size(), mFutures.data(), 0);
                    if (status == wgpu::WaitStatus::TimedOut) {
                        continue;
                    }
                    ASSERT_TRUE(status == wgpu::WaitStatus::Success);
                    bool mayHaveCompletedEarly = IsSpontaneous();
                    if (!mayHaveCompletedEarly && !emptyWait) {
                        ASSERT_GT(mCallbacksCompletedCount, oldCompletionCount);
                    }

                    RemoveCompletedFutures();
                    if (loopOnlyOnce) {
                        break;
                    }
                } while (mFutures.size() > 0);
            } break;
            case WaitType::SpinProcessEvents: {
                do {
                    ASSERT_FALSE(testTimeExceeded());

                    FlushWire();
                    device.Tick();
                    GetInstance().ProcessEvents();

                    if (loopOnlyOnce) {
                        break;
                    }
                } while (mCallbacksCompletedCount < mCallbacksIssuedCount);
            } break;
        }

        if (!IsSpontaneous()) {
            ASSERT_EQ(mCallbacksCompletedCount - oldCompletedCount,
                      mCallbacksIssuedCount - mCallbacksWaitedCount);
        }
        ASSERT_EQ(mCallbacksCompletedCount, mCallbacksIssuedCount);
        mCallbacksWaitedCount = mCallbacksCompletedCount;
    }

    void RemoveCompletedFutures() {
        size_t oldSize = mFutures.size();
        if (oldSize > 0) {
            mFutures.erase(
                std::remove_if(mFutures.begin(), mFutures.end(),
                               [](const wgpu::FutureWaitInfo& info) { return info.completed; }),
                mFutures.end());
            ASSERT_LT(mFutures.size(), oldSize);
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
TEST_P(EventTests, WorkDone_OutOfOrder) {
    // With ProcessEvents or Spontaneous we can't control the order of completion.
    DAWN_TEST_UNSUPPORTED_IF(GetCallbackMode() &
                             (wgpu::CallbackMode::ProcessEvents | wgpu::CallbackMode::Spontaneous));

    TrivialSubmit();
    wgpu::Future f1 = OnSubmittedWorkDone(WGPUQueueWorkDoneStatus_Success);
    TrivialSubmit();
    wgpu::Future f2 = OnSubmittedWorkDone(WGPUQueueWorkDoneStatus_Success);

    // When using WaitAny, normally callback ordering guarantees would guarantee f1 completes before
    // f2. But if we wait on f2 first, then f2 is allowed to complete first because f1 still hasn't
    // had an opportunity to complete.
    TrackForTest(f2);
    TestWaitAll();
    TrackForTest(f1);
    TestWaitAll(true);
}

// TODO(crbug.com/dawn/1987):
// - Test if we make any reentrancy guarantees (for ProcessEvents or WaitAny inside a callback),
//   to make sure things don't blow up and we don't attempt to hold locks recursively.
// - Other tests?

DAWN_INSTANTIATE_TEST_P(EventTests,
                        // TODO(crbug.com/dawn/1987): Enable tests for the rest of the backends.
                        {D3D12Backend(), MetalBackend()},
                        {
                            WaitTypeAndCallbackMode::TimedWaitAny_Future,
                            WaitTypeAndCallbackMode::TimedWaitAny_FutureSpontaneous,
                            WaitTypeAndCallbackMode::SpinWaitAny_Future,
                            WaitTypeAndCallbackMode::SpinWaitAny_FutureSpontaneous,
                            WaitTypeAndCallbackMode::SpinProcessEvents_ProcessEvents,
                            WaitTypeAndCallbackMode::SpinProcessEvents_ProcessEventsSpontaneous,

                            // TODO(crbug.com/dawn/1987): The cases with the Spontaneous flag
                            // enabled were added before we implemented all of the spontaneous
                            // completions. They might accidentally be overly strict.

                            // TODO(crbug.com/dawn/1987): Make guarantees that Spontaneous callbacks
                            // get called (as long as you're hitting "checkpoints"), and add the
                            // corresponding tests, for example:
                            // - SpinProcessEvents_Spontaneous,
                            // - SpinSubmit_Spontaneous,
                            // - SpinTick_Spontaneous (while Dawn still has Tick),
                            // - SpinCheckpoint_Spontaneous (if wgpuDeviceCheckpoint is added).
                        });

}  // anonymous namespace
}  // namespace dawn
