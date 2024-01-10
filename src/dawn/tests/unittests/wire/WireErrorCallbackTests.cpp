// Copyright 2019 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <memory>

#include "dawn/tests/unittests/wire/WireFutureTest.h"
#include "dawn/tests/unittests/wire/WireTest.h"
#include "dawn/wire/WireClient.h"

namespace dawn::wire {
namespace {

using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::SaveArg;
using testing::StrEq;
using testing::StrictMock;

// Mock classes to add expectations on the wire calling callbacks
class MockDeviceErrorCallback {
  public:
    MOCK_METHOD(void, Call, (WGPUErrorType type, const char* message, void* userdata));
};

std::unique_ptr<StrictMock<MockDeviceErrorCallback>> mockDeviceErrorCallback;
void ToMockDeviceErrorCallback(WGPUErrorType type, const char* message, void* userdata) {
    mockDeviceErrorCallback->Call(type, message, userdata);
}

class MockDevicePopErrorScopeCallback {
  public:
    MOCK_METHOD(void, Call, (WGPUErrorType type, const char* message, void* userdata));
};

std::unique_ptr<StrictMock<MockDevicePopErrorScopeCallback>> mockDevicePopErrorScopeCallback;
void ToMockDevicePopErrorScopeCallback(WGPUErrorType type, const char* message, void* userdata) {
    mockDevicePopErrorScopeCallback->Call(type, message, userdata);
}

class MockDeviceLoggingCallback {
  public:
    MOCK_METHOD(void, Call, (WGPULoggingType type, const char* message, void* userdata));
};

std::unique_ptr<StrictMock<MockDeviceLoggingCallback>> mockDeviceLoggingCallback;
void ToMockDeviceLoggingCallback(WGPULoggingType type, const char* message, void* userdata) {
    mockDeviceLoggingCallback->Call(type, message, userdata);
}

class MockDeviceLostCallback {
  public:
    MOCK_METHOD(void, Call, (WGPUDeviceLostReason reason, const char* message, void* userdata));
};

std::unique_ptr<StrictMock<MockDeviceLostCallback>> mockDeviceLostCallback;
void ToMockDeviceLostCallback(WGPUDeviceLostReason reason, const char* message, void* userdata) {
    mockDeviceLostCallback->Call(reason, message, userdata);
}

class WireErrorCallbackTests : public WireTest {
  public:
    WireErrorCallbackTests() {}
    ~WireErrorCallbackTests() override = default;

    void SetUp() override {
        WireTest::SetUp();

        mockDeviceErrorCallback = std::make_unique<StrictMock<MockDeviceErrorCallback>>();
        mockDeviceLoggingCallback = std::make_unique<StrictMock<MockDeviceLoggingCallback>>();
        mockDevicePopErrorScopeCallback =
            std::make_unique<StrictMock<MockDevicePopErrorScopeCallback>>();
        mockDeviceLostCallback = std::make_unique<StrictMock<MockDeviceLostCallback>>();
    }

    void TearDown() override {
        WireTest::TearDown();

        mockDeviceErrorCallback = nullptr;
        mockDeviceLoggingCallback = nullptr;
        mockDevicePopErrorScopeCallback = nullptr;
        mockDeviceLostCallback = nullptr;
    }

    void FlushServer() {
        WireTest::FlushServer();

        Mock::VerifyAndClearExpectations(&mockDeviceErrorCallback);
        Mock::VerifyAndClearExpectations(&mockDevicePopErrorScopeCallback);
    }
};

// Test the return wire for device validation error callbacks
TEST_F(WireErrorCallbackTests, DeviceValidationErrorCallback) {
    wgpuDeviceSetUncapturedErrorCallback(device, ToMockDeviceErrorCallback, this);

    // Setting the error callback should stay on the client side and do nothing
    FlushClient();

    // Calling the callback on the server side will result in the callback being called on the
    // client side
    api.CallDeviceSetUncapturedErrorCallbackCallback(apiDevice, WGPUErrorType_Validation,
                                                     "Some error message");

    EXPECT_CALL(*mockDeviceErrorCallback,
                Call(WGPUErrorType_Validation, StrEq("Some error message"), this))
        .Times(1);

    FlushServer();
}

// Test the return wire for device OOM error callbacks
TEST_F(WireErrorCallbackTests, DeviceOutOfMemoryErrorCallback) {
    wgpuDeviceSetUncapturedErrorCallback(device, ToMockDeviceErrorCallback, this);

    // Setting the error callback should stay on the client side and do nothing
    FlushClient();

    // Calling the callback on the server side will result in the callback being called on the
    // client side
    api.CallDeviceSetUncapturedErrorCallbackCallback(apiDevice, WGPUErrorType_OutOfMemory,
                                                     "Some error message");

    EXPECT_CALL(*mockDeviceErrorCallback,
                Call(WGPUErrorType_OutOfMemory, StrEq("Some error message"), this))
        .Times(1);

    FlushServer();
}

// Test the return wire for device internal error callbacks
TEST_F(WireErrorCallbackTests, DeviceInternalErrorCallback) {
    wgpuDeviceSetUncapturedErrorCallback(device, ToMockDeviceErrorCallback, this);

    // Setting the error callback should stay on the client side and do nothing
    FlushClient();

    // Calling the callback on the server side will result in the callback being called on the
    // client side
    api.CallDeviceSetUncapturedErrorCallbackCallback(apiDevice, WGPUErrorType_Internal,
                                                     "Some error message");

    EXPECT_CALL(*mockDeviceErrorCallback,
                Call(WGPUErrorType_Internal, StrEq("Some error message"), this))
        .Times(1);

    FlushServer();
}

// Test the return wire for device user warning callbacks
TEST_F(WireErrorCallbackTests, DeviceLoggingCallback) {
    wgpuDeviceSetLoggingCallback(device, ToMockDeviceLoggingCallback, this);

    // Setting the injected warning callback should stay on the client side and do nothing
    FlushClient();

    // Calling the callback on the server side will result in the callback being called on the
    // client side
    api.CallDeviceSetLoggingCallbackCallback(apiDevice, WGPULoggingType_Info, "Some message");

    EXPECT_CALL(*mockDeviceLoggingCallback, Call(WGPULoggingType_Info, StrEq("Some message"), this))
        .Times(1);

    FlushServer();
}

// Test the return wire for device lost callback
TEST_F(WireErrorCallbackTests, DeviceLostCallback) {
    wgpuDeviceSetDeviceLostCallback(device, ToMockDeviceLostCallback, this);

    // Setting the error callback should stay on the client side and do nothing
    FlushClient();

    // Calling the callback on the server side will result in the callback being called on the
    // client side
    api.CallDeviceSetDeviceLostCallbackCallback(apiDevice, WGPUDeviceLostReason_Undefined,
                                                "Some error message");

    EXPECT_CALL(*mockDeviceLostCallback,
                Call(WGPUDeviceLostReason_Undefined, StrEq("Some error message"), this))
        .Times(1);

    FlushServer();
}

// TODO(crbug.com/dawn/2021) Use the new callback signature when possible.
class WirePopErrorScopeCallbackTests : public WireFutureTest<WGPUErrorCallback,
                                                             WGPUPopErrorScopeCallbackInfo,
                                                             wgpuDevicePopErrorScope,
                                                             wgpuDevicePopErrorScopeF> {
  protected:
    // Overridden version of wgpuDevicePopErrorScope that defers to the API call based on hte test
    // callback mode.
    void DevicePopErrorScope(WGPUDevice d, void* userdata = nullptr) { CallImpl(userdata, d); }

    void PushErrorScope(WGPUErrorFilter filter) {
        EXPECT_CALL(api, DevicePushErrorScope(apiDevice, filter)).Times(1);
        wgpuDevicePushErrorScope(device, filter);
        FlushClient();
    }
};
DAWN_INSTANTIATE_WIRE_FUTURE_TEST_P(WirePopErrorScopeCallbackTests);

// Test the return wire for validation error scopes.
TEST_P(WirePopErrorScopeCallbackTests, TypeAndFilters) {
    static constexpr std::array<std::pair<WGPUErrorType, WGPUErrorFilter>, 3> kErrorTypeAndFilters =
        {{{WGPUErrorType_Validation, WGPUErrorFilter_Validation},
          {WGPUErrorType_OutOfMemory, WGPUErrorFilter_OutOfMemory},
          {WGPUErrorType_Internal, WGPUErrorFilter_Internal}}};

    for (const auto& [type, filter] : kErrorTypeAndFilters) {
        PushErrorScope(filter);

        DevicePopErrorScope(device, this);
        EXPECT_CALL(api, OnDevicePopErrorScope(apiDevice, _, _)).WillOnce(InvokeWithoutArgs([&] {
            api.CallDevicePopErrorScopeCallback(apiDevice, type, "Some error message");
        }));

        FlushClient();
        FlushFutures();
        ExpectWireCallbacksWhen([&](auto& mockCb) {
            EXPECT_CALL(mockCb, Call(type, StrEq("Some error message"), this)).Times(1);

            FlushCallbacks();
        });
    }
}

// Registering a callback then wire disconnect calls the callback with Unknown error type.
// TODO(crbug.com/dawn/2021) When using new callback signature, check for InstanceDropped status.
TEST_P(WirePopErrorScopeCallbackTests, Disconnect) {
    PushErrorScope(WGPUErrorFilter_Validation);

    DevicePopErrorScope(device, this);
    EXPECT_CALL(api, OnDevicePopErrorScope(apiDevice, _, _)).WillOnce(InvokeWithoutArgs([&] {
        api.CallDevicePopErrorScopeCallback(apiDevice, WGPUErrorType_Validation,
                                            "Some error message");
    }));

    FlushClient();
    FlushFutures();
    ExpectWireCallbacksWhen([&](auto& mockCb) {
        EXPECT_CALL(mockCb, Call(WGPUErrorType_Unknown, nullptr, this)).Times(1);

        GetWireClient()->Disconnect();
    });
}

// Test that registering a callback after wire disconnect calls the callback with
// DeviceLost.
TEST_F(WireErrorCallbackTests, PopErrorScopeAfterDisconnect) {
    EXPECT_CALL(api, DevicePushErrorScope(apiDevice, WGPUErrorFilter_Validation)).Times(1);
    wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
    FlushClient();

    GetWireClient()->Disconnect();

    EXPECT_CALL(*mockDevicePopErrorScopeCallback,
                Call(WGPUErrorType_DeviceLost, ValidStringMessage(), this))
        .Times(1);
    wgpuDevicePopErrorScope(device, ToMockDevicePopErrorScopeCallback, this);
}

// Empty stack (We are emulating the errors that would be callback-ed from native).
TEST_F(WireErrorCallbackTests, PopErrorScopeEmptyStack) {
    WGPUErrorCallback callback;
    void* userdata;
    EXPECT_CALL(api, OnDevicePopErrorScope(apiDevice, _, _))
        .WillOnce(DoAll(SaveArg<1>(&callback), SaveArg<2>(&userdata)));
    wgpuDevicePopErrorScope(device, ToMockDevicePopErrorScopeCallback, this);
    FlushClient();

    EXPECT_CALL(*mockDevicePopErrorScopeCallback,
                Call(WGPUErrorType_Validation, StrEq("No error scopes to pop"), this))
        .Times(1);
    callback(WGPUErrorType_Validation, "No error scopes to pop", userdata);
    FlushServer();
}

}  // anonymous namespace
}  // namespace dawn::wire
