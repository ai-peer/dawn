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

#ifndef SRC_DAWN_TESTS_UNITTESTS_WIRE_WIREFUTURETEST_H_
#define SRC_DAWN_TESTS_UNITTESTS_WIRE_WIREFUTURETEST_H_

#include <string>
#include <utility>
#include <vector>

#include "dawn/common/FutureUtils.h"
#include "dawn/tests/unittests/wire/WireTest.h"
#include "dawn/wire/WireServer.h"

#include "gtest/gtest.h"

#include "webgpu/webgpu_cpp.h"

namespace dawn::wire {

enum class CallbackMode {
    Async,  // Legacy mode that internally defers to Spontaneous.
    WaitAny,
    ProcessEvents,
    Spontaneous,
};

static constexpr std::array<CallbackMode, 4> kCallbackModes = {
    CallbackMode::Async, CallbackMode::WaitAny, CallbackMode::ProcessEvents,
    CallbackMode::Spontaneous};

WGPUCallbackMode ToWGPUCallbackMode(CallbackMode callbackMode);
std::string CallbackModeParamName(const testing::TestParamInfo<CallbackMode>& info);

#define DAWN_INSTANTIATE_WIRE_FUTURE_TEST_P(testName) \
    INSTANTIATE_TEST_SUITE_P(, testName, testing::ValuesIn(kCallbackModes), &CallbackModeParamName)

template <typename Callback,
          typename CallbackInfo,
          auto& AsyncF,
          auto& FutureF,
          typename AsyncFT = decltype(AsyncF),
          typename FutureFT = decltype(FutureF)>
class WireFutureTest : public WireTest, public testing::WithParamInterface<CallbackMode> {
  protected:
    void SetUp() override {
        WireTest::SetUp();

        auto reservation = GetWireClient()->ReserveInstance();
        instance = wgpu::Instance::Acquire(reservation.instance);

        apiInstance = api.GetNewInstance();
        EXPECT_CALL(api, InstanceReference(apiInstance));
        EXPECT_TRUE(
            GetWireServer()->InjectInstance(apiInstance, reservation.id, reservation.generation));
    }

    void TearDown() override {
        instance = nullptr;
        WireTest::TearDown();
    }

    // Calls the actual API that the test suite is exercising given the callback mode. This should
    // be used in favor of directly calling the API because the Async mode actually calls a
    // different entry point.
    template <typename... Args>
    void CallImpl(Callback cb, void* userdata, Args&&... args) {
        if (GetParam() == CallbackMode::Async) {
            mAsyncF(std::forward<Args>(args)..., cb, userdata);
        } else {
            CallbackInfo callbackInfo = {};
            callbackInfo.mode = ToWGPUCallbackMode(GetParam());
            callbackInfo.callback = cb;
            callbackInfo.userdata = userdata;
            mFutureIDs.push_back(mFutureF(std::forward<Args>(args)..., callbackInfo).id);
        }
    }

    // Future suite flushes are designed such that the general use-case would look as follows:
    //
    //     // Call the API under test
    //     CallImpl(mockCb, this, args...);
    //     EXPECT_CALL(api, OnAsyncAPI(...)).WillOnce(InvokeWithoutArgs([&] {
    //         api.CallAsyncAPICallback(...);
    //     }));
    //
    //     FlushClientFutures();  // Eensures the callback is ready but NOT called.
    //     EXPECT_CALL(mockCb, Call(...));
    //     FlushServerFutures();  // Calls the callbacks
    //
    // The use case above will verify that the callback is called correctly w.r.t the callback mode,
    // otherwise `mockCb` will trigger an unexpected call. Test writers can use these flush helpers
    // in conjunction with the WireTest::Flush* functions to force specific behaviors about whether
    // the server replied to the client regarding a future operation or not.
    //
    // WireTest::FlushClient will ensure that the client call is forwarded to the server, but the
    // server will NOT have replied yet.
    //
    // WireTest::FlushServer will force the server to reply to the client marking Futures ready or
    // even calling them in the case of Async or Spontaneous. For other cases, the callbacks will
    // NOT have been called yet.
    void FlushClientFutures() {
        WireTest::FlushClient();

        // Based on the callback type, we want to do some server level flushes to help ensure that
        // the callbacks are only called when they are supposed to. For the legacy Async calls and
        // the Spontaneous calls, we don't want to flush the server yet because that would trigger
        // the callbacks. Otherwise, we want to flush the server as well to ensure that the
        // callbacks are not called.
        CallbackMode callbackMode = GetParam();
        if (callbackMode == CallbackMode::WaitAny || callbackMode == CallbackMode::ProcessEvents) {
            WireTest::FlushServer();
        }
    }
    void FlushServerFutures() {
        // Based on the callback type, we may call a synchronization entry point here and flush both
        // ends to make sure that the callbacks are called.
        CallbackMode callbackMode = GetParam();
        if (callbackMode == CallbackMode::WaitAny) {
            if (mFutureIDs.empty()) {
                WireTest::FlushServer();
                return;
            }
            std::vector<wgpu::FutureWaitInfo> waitInfos;
            for (auto futureID : mFutureIDs) {
                waitInfos.push_back({{futureID}, false});
            }
            EXPECT_EQ(instance.WaitAny(mFutureIDs.size(), waitInfos.data(), 0),
                      wgpu::WaitStatus::Success);
        } else if (callbackMode == CallbackMode::ProcessEvents) {
            instance.ProcessEvents();
        }

        WireTest::FlushServer();
    }

    wgpu::Instance instance;
    WGPUInstance apiInstance;

  private:
    AsyncFT mAsyncF = AsyncF;
    FutureFT mFutureF = FutureF;
    std::vector<FutureID> mFutureIDs;
};

}  // namespace dawn::wire

#endif  // SRC_DAWN_TESTS_UNITTESTS_WIRE_WIREFUTURETEST_H_
