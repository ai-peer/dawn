// Copyright 2020 The Dawn Authors
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

#include "tests/unittests/wire/WireTest.h"

#include "common/Assert.h"
#include "dawn_wire/WireClient.h"

#include <memory>
#include <set>

using namespace testing;
using namespace dawn_wire;

namespace {

    template <typename F>
    class MockCallback;

    // Helper class for mocking callbacks. TODO(enga): Move to a common header
    // and use it for other callback mocks.
    //
    // Example Usage:
    //   MockCallback<WGPUDeviceLostCallback> mock;
    //
    //   void* foo = XYZ; // this is the callback userdata
    //
    //   wgpuDeviceSetDeviceLostCallback(device, mock.Callback(), mock.MakeUserdata(foo));
    //   EXPECT_CALL(mock, Call(_, foo));
    template <typename R, typename... Args>
    class MockCallback<R (*)(Args...)> : public ::testing::MockFunction<R(Args...)> {
      public:
        auto Callback() {
            return CallUnboundCallback;
        }

        void* MakeUserdata(void* userdata) {
            auto* mockAndUserdata = new MockAndUserdata{this, userdata};

            // Add the userdata to a set of userdata for this mock. We never
            // remove from this set even if a callback should only be called once so that
            // repeated calls to the callback still forward the userdata correctly.
            // Userdata will be destroyed when the mock is destroyed.
            auto it = mUserdatas.emplace(mockAndUserdata);
            ASSERT(it.second);
            return it.first->get();
        }

      private:
        struct MockAndUserdata {
            MockCallback* mock;
            void* userdata;
        };

        static R CallUnboundCallback(Args... args) {
            std::tuple<Args...> tuple = std::make_tuple(args...);

            constexpr size_t ArgC = sizeof...(Args);
            static_assert(ArgC >= 1, "Mock callback requires at least one argument (the userdata)");

            // Get the userdata. It should be the last argument.
            auto userdata = std::get<ArgC - 1>(tuple);
            static_assert(std::is_same<decltype(userdata), void*>::value,
                          "Last callback argument must be void* userdata");

            // Extract the mock.
            ASSERT(userdata != nullptr);
            auto* mockAndUserdata = reinterpret_cast<MockAndUserdata*>(userdata);
            MockCallback* mock = mockAndUserdata->mock;
            ASSERT(mock != nullptr);

            // Replace the userdata
            std::get<ArgC - 1>(tuple) = mockAndUserdata->userdata;

            // Forward the callback to the mock.
            return mock->CallImpl(std::make_index_sequence<ArgC>{}, std::move(tuple));
        }

        template <size_t... Is>
        R CallImpl(const std::index_sequence<Is...>&, std::tuple<Args...> args) {
            return this->Call(std::get<Is>(args)...);
        }

        std::set<std::unique_ptr<MockAndUserdata>> mUserdatas;
    };

    class WireDisconnectTests : public WireTest {
      protected:
        MockCallback<WGPUDeviceLostCallback> mockDeviceLostCallback;
    };

}  // anonymous namespace

// Test that commands are not received if the client disconnects.
TEST_F(WireDisconnectTests, CommandsAfterDisconnect) {
    // Sanity check that commands work at all.
    wgpuDeviceCreateCommandEncoder(device, nullptr);

    WGPUCommandEncoder apiCmdBufEncoder = api.GetNewCommandEncoder();
    EXPECT_CALL(api, DeviceCreateCommandEncoder(apiDevice, nullptr))
        .WillOnce(Return(apiCmdBufEncoder));
    FlushClient();

    // Disconnect.
    GetWireClient()->Disconnect();

    // Command is not received because client disconnected.
    wgpuDeviceCreateCommandEncoder(device, nullptr);
    EXPECT_CALL(api, DeviceCreateCommandEncoder(_, _)).Times(Exactly(0));
    FlushClient();
}

// Test that commands that are serialized before a disconnect but flushed
// after are received.
TEST_F(WireDisconnectTests, FlushAfterDisconnect) {
    // Sanity check that commands work at all.
    wgpuDeviceCreateCommandEncoder(device, nullptr);

    // Disconnect.
    GetWireClient()->Disconnect();

    // Already-serialized commmands are still received.
    WGPUCommandEncoder apiCmdBufEncoder = api.GetNewCommandEncoder();
    EXPECT_CALL(api, DeviceCreateCommandEncoder(apiDevice, nullptr))
        .WillOnce(Return(apiCmdBufEncoder));
    FlushClient();
}

// Check that disconnecting the wire client calls the device lost callback exacty once.
TEST_F(WireDisconnectTests, CallsDeviceLostCallback) {
    wgpuDeviceSetDeviceLostCallback(device, mockDeviceLostCallback.Callback(),
                                    mockDeviceLostCallback.MakeUserdata(this));

    // Disconnect the wire client. We should receive device lost only once.
    EXPECT_CALL(mockDeviceLostCallback, Call(_, this)).Times(Exactly(1));
    GetWireClient()->Disconnect();
    GetWireClient()->Disconnect();
}

// Check that disconnecting the wire client after a device loss does not trigger the callback again.
TEST_F(WireDisconnectTests, ServerLostThenDisconnect) {
    wgpuDeviceSetDeviceLostCallback(device, mockDeviceLostCallback.Callback(),
                                    mockDeviceLostCallback.MakeUserdata(this));

    api.CallDeviceLostCallback(apiDevice, "some reason");

    // Flush the device lost return command.
    EXPECT_CALL(mockDeviceLostCallback, Call(StrEq("some reason"), this)).Times(Exactly(1));
    FlushServer();

    // Disconnect the client. We shouldn't see the lost callback again.
    EXPECT_CALL(mockDeviceLostCallback, Call(_, _)).Times(Exactly(0));
    GetWireClient()->Disconnect();
}

// Check that disconnecting the wire client inside the device loss callback does not trigger the
// callback again.
TEST_F(WireDisconnectTests, ServerLostThenDisconnectInCallback) {
    wgpuDeviceSetDeviceLostCallback(device, mockDeviceLostCallback.Callback(),
                                    mockDeviceLostCallback.MakeUserdata(this));

    api.CallDeviceLostCallback(apiDevice, "lost reason");

    // Disconnect the client inside the lost callback. We should see the callback
    // only once.
    EXPECT_CALL(mockDeviceLostCallback, Call(StrEq("lost reason"), this))
        .WillOnce(InvokeWithoutArgs([&]() {
            EXPECT_CALL(mockDeviceLostCallback, Call(_, _)).Times(Exactly(0));
            GetWireClient()->Disconnect();
        }));
    FlushServer();
}

// Check that a device loss after a disconnect does not trigger the callback again.
TEST_F(WireDisconnectTests, DisconnectThenServerLost) {
    wgpuDeviceSetDeviceLostCallback(device, mockDeviceLostCallback.Callback(),
                                    mockDeviceLostCallback.MakeUserdata(this));

    // Disconnect the client. We should see the callback once.
    EXPECT_CALL(mockDeviceLostCallback, Call(_, this)).Times(Exactly(1));
    GetWireClient()->Disconnect();

    // Lose the device on the server. The client callback shouldn't be
    // called again.
    api.CallDeviceLostCallback(apiDevice, "lost reason");
    EXPECT_CALL(mockDeviceLostCallback, Call(_, _)).Times(Exactly(0));
    FlushServer();
}
