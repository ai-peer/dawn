// Copyright 2019 The Dawn Authors
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
#include "dawn/dawn_proc.h"
#include "dawn_wire/WireClient.h"
#include "dawn_wire/WireServer.h"
#include "utils/TerribleCommandBuffer.h"

#include <array>

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

}  // anonymous namespace

class WireMultipleDeviceTests : public testing::Test {
  protected:
    void SetUp() override {
        DawnProcTable procs = dawn_wire::WireClient::GetProcs();
        dawnProcSetProcs(&procs);
    }

    void TearDown() override {
        dawnProcSetProcs(nullptr);
    }

    class WireHolder {
      public:
        WireHolder() {
            DawnProcTable mockProcs;
            mApi.GetProcTableAndDevice(&mockProcs, &mServerDevice);

            // Ignore Tick()
            EXPECT_CALL(mApi, DeviceTick(_)).Times(AnyNumber());

            // This SetCallback call cannot be ignored because it is done as soon as we start the
            // server
            EXPECT_CALL(mApi, OnDeviceSetUncapturedErrorCallback(_, _, _)).Times(Exactly(1));
            EXPECT_CALL(mApi, OnDeviceSetDeviceLostCallback(_, _, _)).Times(Exactly(1));

            mS2cBuf = std::make_unique<utils::TerribleCommandBuffer>();
            mC2sBuf = std::make_unique<utils::TerribleCommandBuffer>(mWireServer.get());

            WireServerDescriptor serverDesc = {};
            serverDesc.device = mServerDevice;
            serverDesc.procs = &mockProcs;
            serverDesc.serializer = mS2cBuf.get();

            mWireServer.reset(new WireServer(serverDesc));
            mC2sBuf->SetHandler(mWireServer.get());

            WireClientDescriptor clientDesc = {};
            clientDesc.serializer = mC2sBuf.get();

            mWireClient.reset(new WireClient(clientDesc));
            mS2cBuf->SetHandler(mWireClient.get());

            mClientDevice = mWireClient->GetDevice();
        }

        ~WireHolder() {
            mApi.IgnoreAllReleaseCalls();
            mWireClient = nullptr;
            mWireServer = nullptr;
        }

        void FlushClient(bool success = true) {
            ASSERT_EQ(mC2sBuf->Flush(), success);
        }

        void FlushServer(bool success = true) {
            ASSERT_EQ(mS2cBuf->Flush(), success);
        }

        testing::StrictMock<MockProcTable>* Api() {
            return &mApi;
        }

        WGPUDevice ClientDevice() {
            return mClientDevice;
        }

        WGPUDevice ServerDevice() {
            return mServerDevice;
        }

      private:
        testing::StrictMock<MockProcTable> mApi;
        std::unique_ptr<dawn_wire::WireServer> mWireServer;
        std::unique_ptr<dawn_wire::WireClient> mWireClient;
        std::unique_ptr<utils::TerribleCommandBuffer> mS2cBuf;
        std::unique_ptr<utils::TerribleCommandBuffer> mC2sBuf;
        WGPUDevice mServerDevice;
        WGPUDevice mClientDevice;
    };
};

// Test that using objects from a difference device is a validation error.
TEST_F(WireMultipleDeviceTests, ValidatesSameDevice) {
    WireHolder wireA;
    WireHolder wireB;

    // Create the objects
    WGPUQueue queueA = wgpuDeviceCreateQueue(wireA.ClientDevice());
    WGPUQueue queueB = wgpuDeviceCreateQueue(wireB.ClientDevice());

    WGPUFenceDescriptor desc = {};
    WGPUFence fenceA = wgpuQueueCreateFence(queueA, &desc);

    // Flush on wire B. We should see the queue created.
    EXPECT_CALL(*wireB.Api(), DeviceCreateQueue(wireB.ServerDevice()))
        .WillOnce(Return(wireB.Api()->GetNewQueue()));
    wireB.FlushClient();

    // Signal with a fence from a different wire.
    wgpuQueueSignal(queueB, fenceA, 1u);

    // We should inject an error into the server.
    std::string errorMessage;
    EXPECT_CALL(*wireB.Api(), DeviceInjectError(wireB.ServerDevice(), WGPUErrorType_Validation, _))
        .WillOnce(Invoke([&](WGPUDevice device, WGPUErrorType type, const char* message) {
            errorMessage = message;
            // Mock the call to the error callback.
            wireB.Api()->CallDeviceErrorCallback(device, type, message);
        }));
    wireB.FlushClient();

    // The error callback should be forwarded to the client.
    StrictMock<MockCallback<WGPUErrorCallback>> mockErrorCallback;
    wgpuDeviceSetUncapturedErrorCallback(wireB.ClientDevice(), mockErrorCallback.Callback(),
                                         mockErrorCallback.MakeUserdata(this));

    EXPECT_CALL(mockErrorCallback, Call(WGPUErrorType_Validation, StrEq(errorMessage), this))
        .Times(1);
    wireB.FlushServer();
}

// Test that objects created from mixed devices are an error to use.
TEST_F(WireMultipleDeviceTests, DifferentDeviceObjectCreationIsError) {
    WireHolder wireA;
    WireHolder wireB;

    // Create a bind group layout on wire A.
    WGPUBindGroupLayoutDescriptor bglDesc = {};
    WGPUBindGroupLayout bglA = wgpuDeviceCreateBindGroupLayout(wireA.ClientDevice(), &bglDesc);
    EXPECT_CALL(*wireA.Api(), DeviceCreateBindGroupLayout(wireA.ServerDevice(), _))
        .WillOnce(Return(wireA.Api()->GetNewBindGroupLayout()));

    wireA.FlushClient();

    std::array<WGPUBindGroupBinding, 2> bindings;

    // Create a buffer on wire A.
    WGPUBufferDescriptor bufferDesc = {};
    bindings[0].buffer = wgpuDeviceCreateBuffer(wireA.ClientDevice(), &bufferDesc);
    EXPECT_CALL(*wireA.Api(), DeviceCreateBuffer(wireA.ServerDevice(), _))
        .WillOnce(Return(wireA.Api()->GetNewBuffer()));

    wireA.FlushClient();

    // Create a sampler on wire B.
    WGPUSamplerDescriptor samplerDesc = {};
    bindings[1].sampler = wgpuDeviceCreateSampler(wireB.ClientDevice(), &samplerDesc);
    EXPECT_CALL(*wireB.Api(), DeviceCreateSampler(wireB.ServerDevice(), _))
        .WillOnce(Return(wireB.Api()->GetNewSampler()));

    wireB.FlushClient();

    // Create a bind group on wire A using the bgl (A), buffer (A), and sampler (B).
    WGPUBindGroupDescriptor bgDesc = {};
    bgDesc.layout = bglA;
    bgDesc.bindingCount = bindings.size();
    bgDesc.bindings = bindings.data();
    WGPUBindGroup bindGroupA = wgpuDeviceCreateBindGroup(wireA.ClientDevice(), &bgDesc);

    // It should inject an error because the sampler is from a different device.
    std::string errorMessage;
    EXPECT_CALL(*wireA.Api(), DeviceInjectError(wireA.ServerDevice(), WGPUErrorType_Validation, _))
        .WillOnce(Invoke([&](WGPUDevice device, WGPUErrorType type, const char* message) {
            errorMessage = message;
            // Mock the call to the error callback.
            wireA.Api()->CallDeviceErrorCallback(device, type, message);
        }));

    wireA.FlushClient();

    // The error callback should be forwarded to the client.
    StrictMock<MockCallback<WGPUErrorCallback>> mockErrorCallback;
    wgpuDeviceSetUncapturedErrorCallback(wireA.ClientDevice(), mockErrorCallback.Callback(),
                                         mockErrorCallback.MakeUserdata(this));

    EXPECT_CALL(mockErrorCallback, Call(WGPUErrorType_Validation, StrEq(errorMessage), this))
        .Times(1);
    wireA.FlushServer();

    // The bind group was never created on a server because it failed device validation.
    // Any commands that use it should error.
    wgpuBindGroupRelease(bindGroupA);
    wireA.FlushClient(false);
}
