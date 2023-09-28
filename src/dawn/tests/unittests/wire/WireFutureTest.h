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
#include "dawn/tests/ParamGenerator.h"
#include "dawn/tests/unittests/wire/WireTest.h"
#include "dawn/wire/WireServer.h"

#include "gtest/gtest.h"

namespace dawn::wire {

enum class CallbackMode {
    Async,  // Legacy mode that internally defers to Spontaneous.
    WaitAny,
    ProcessEvents,
    Spontaneous,
};
WGPUCallbackMode ToWGPUCallbackMode(CallbackMode callbackMode);

struct WireFutureTestParam {
    // NOLINTNEXTLINE(runtime/explicit)
    WireFutureTestParam(CallbackMode callbackMode);
    CallbackMode mCallbackMode;
};
std::ostream& operator<<(std::ostream& os, const WireFutureTestParam& param);
std::vector<WireFutureTestParam>& CallbackModes();

template <typename Param, typename... Params>
auto MakeParamGenerator(std::initializer_list<Params>&&... params) {
    return ParamGenerator<Param, WireFutureTestParam, Params...>(
        CallbackModes(), std::forward<std::initializer_list<Params>&&>(params)...);
}

// Usage: DAWN_WIRE_FUTURE_TEST_PARAM_STRUCT(Foo, TypeA, TypeB, ...)
// Generate a test param struct called Foo which extends WireFutureTestParam and generated
// struct _Dawn_Foo. _Dawn_Foo has members of types TypeA, TypeB, etc. which are named mTypeA,
// mTypeB, etc. in the order they are placed in the macro argument list. Struct Foo should be
// constructed with an WireFutureTestParam as the first argument, followed by a list of values
// to initialize the base _Dawn_Foo struct.
// It is recommended to use alias declarations so that stringified types are more readable.
// Example:
//   using MyParam = unsigned int;
//   DAWN_WIRE_FUTURE_TEST_PARAM_STRUCT(FooParams, MyParam);
#define DAWN_WIRE_FUTURE_TEST_PARAM_STRUCT(StructName, ...) \
    DAWN_TEST_PARAM_STRUCT_BASE(WireFutureTestParam, StructName, __VA_ARGS__)

#define DAWN_INSTANTIATE_WIRE_FUTURE_TEST_P(testName, ...)                                   \
    INSTANTIATE_TEST_SUITE_P(                                                                \
        , testName, testing::ValuesIn(MakeParamGenerator<testName::ParamType>(__VA_ARGS__)), \
        &TestParamToString<testName::ParamType>)

template <typename Callback,
          typename CallbackInfo,
          auto& AsyncF,
          auto& FutureF,
          typename Params = WireFutureTestParam,
          typename AsyncFT = decltype(AsyncF),
          typename FutureFT = decltype(FutureF)>
class WireFutureTestWithParams : public WireTest, public testing::WithParamInterface<Params> {
  protected:
    using testing::WithParamInterface<Params>::GetParam;

    void SetUp() override {
        WireTest::SetUp();

        auto reservation = GetWireClient()->ReserveInstance();
        instance = reservation.instance;

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
        if (GetParam().mCallbackMode == CallbackMode::Async) {
            mAsyncF(std::forward<Args>(args)..., cb, userdata);
        } else {
            CallbackInfo callbackInfo = {};
            callbackInfo.mode = ToWGPUCallbackMode(GetParam().mCallbackMode);
            callbackInfo.callback = cb;
            callbackInfo.userdata = userdata;
            mFutureIDs.push_back(mFutureF(std::forward<Args>(args)..., callbackInfo).id);
        }
    }

    CallbackMode GetCallbackMode() { return GetParam().mCallbackMode; }

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
        CallbackMode callbackMode = GetParam().mCallbackMode;
        if (callbackMode == CallbackMode::WaitAny || callbackMode == CallbackMode::ProcessEvents) {
            WireTest::FlushServer();
        }
    }
    void FlushServerFutures() {
        WireTest::FlushServer();

        // Based on the callback type, we may call a synchronization entry point here and flush the
        // server to make sure that the callbacks are called.
        CallbackMode callbackMode = GetParam().mCallbackMode;
        if (callbackMode == CallbackMode::WaitAny) {
            if (mFutureIDs.empty()) {
                WireTest::FlushServer();
                return;
            }
            std::vector<WGPUFutureWaitInfo> waitInfos;
            for (auto futureID : mFutureIDs) {
                waitInfos.push_back({{futureID}, false});
            }
            EXPECT_EQ(wgpuInstanceWaitAny(instance, mFutureIDs.size(), waitInfos.data(), 0),
                      WGPUWaitStatus_Success);
        } else if (callbackMode == CallbackMode::ProcessEvents) {
            wgpuInstanceProcessEvents(instance);
        }
    }

    WGPUInstance instance;
    WGPUInstance apiInstance;

  private:
    AsyncFT mAsyncF = AsyncF;
    FutureFT mFutureF = FutureF;
    std::vector<FutureID> mFutureIDs;
};

template <typename Callback, typename CallbackInfo, auto& AsyncF, auto& FutureF>
using WireFutureTest = WireFutureTestWithParams<Callback, CallbackInfo, AsyncF, FutureF>;

}  // namespace dawn::wire

#endif  // SRC_DAWN_TESTS_UNITTESTS_WIRE_WIREFUTURETEST_H_
