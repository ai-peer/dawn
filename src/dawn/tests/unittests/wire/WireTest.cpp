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

#include "dawn/tests/unittests/wire/WireTest.h"

#include "dawn/dawn_proc.h"
#include "dawn/tests/MockCallback.h"
#include "dawn/utils/TerribleCommandBuffer.h"
#include "dawn/wire/WireClient.h"
#include "dawn/wire/WireServer.h"

using testing::_;
using testing::AnyNumber;
using testing::Exactly;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::MockCallback;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;
using testing::WithArg;

WireTest::WireTest() {}

WireTest::~WireTest() {}

dawn::wire::client::MemoryTransferService* WireTest::GetClientMemoryTransferService() {
    return nullptr;
}

dawn::wire::server::MemoryTransferService* WireTest::GetServerMemoryTransferService() {
    return nullptr;
}

void WireTest::SetUp() {
    DawnProcTable mockProcs;
    api.GetProcTable(&mockProcs);
    SetupIgnoredCallExpectations();

    mS2cBuf = std::make_unique<dawn::utils::TerribleCommandBuffer>();
    mC2sBuf = std::make_unique<dawn::utils::TerribleCommandBuffer>(mWireServer.get());

    dawn::wire::WireServerDescriptor serverDesc = {};
    serverDesc.procs = &mockProcs;
    serverDesc.serializer = mS2cBuf.get();
    serverDesc.memoryTransferService = GetServerMemoryTransferService();

    mWireServer.reset(new dawn::wire::WireServer(serverDesc));
    mC2sBuf->SetHandler(mWireServer.get());

    dawn::wire::WireClientDescriptor clientDesc = {};
    clientDesc.serializer = mC2sBuf.get();
    clientDesc.memoryTransferService = GetClientMemoryTransferService();

    mWireClient.reset(new dawn::wire::WireClient(clientDesc));
    mS2cBuf->SetHandler(mWireClient.get());

    dawnProcSetProcs(&dawn::wire::client::GetProcs());

    auto reservation = GetWireClient()->ReserveInstance();
    instance = reservation.instance;

    apiInstance = api.GetNewInstance();
    EXPECT_CALL(api, InstanceReference(apiInstance));
    EXPECT_TRUE(
        GetWireServer()->InjectInstance(apiInstance, reservation.id, reservation.generation));

    WGPURequestAdapterOptions options = {};
    MockCallback<WGPURequestAdapterCallback> cb;
    auto* userdata = cb.MakeUserdata(this);
    wgpuInstanceRequestAdapter(instance, &options, cb.Callback(), userdata);

    // Expect the server to receive the message. Then, mock a reply.
    apiAdapter = api.GetNewAdapter();
    EXPECT_CALL(api, OnInstanceRequestAdapter(apiInstance, NotNull(), NotNull(), NotNull()))
        .WillOnce(InvokeWithoutArgs([&] {
            EXPECT_CALL(api, AdapterHasFeature(apiAdapter, _)).WillRepeatedly(Return(false));

            EXPECT_CALL(api, AdapterGetProperties(apiAdapter, NotNull()))
                .WillOnce(WithArg<1>(Invoke([&](WGPUAdapterProperties* properties) {
                    *properties = {};
                    properties->vendorName = "";
                    properties->architecture = "";
                    properties->name = "";
                    properties->driverDescription = "";
                })));

            EXPECT_CALL(api, AdapterGetLimits(apiAdapter, NotNull()))
                .WillOnce(WithArg<1>(Invoke([&](WGPUSupportedLimits* limits) {
                    *limits = {};
                    return true;
                })));

            EXPECT_CALL(api, AdapterEnumerateFeatures(apiAdapter, nullptr))
                .WillOnce(Return(0))
                .WillOnce(Return(0));
            api.CallInstanceRequestAdapterCallback(apiInstance, WGPURequestAdapterStatus_Success,
                                                   apiAdapter, nullptr);
        }));
    FlushClient();

    // Expect the callback in the client.
    EXPECT_CALL(cb, Call(WGPURequestAdapterStatus_Success, NotNull(), nullptr, this))
        .WillOnce(SaveArg<1>(&adapter));
    FlushServer();

    MockCallback<WGPURequestDeviceCallback> requestDeviceCb;

    WGPUDeviceDescriptor deviceDesc = {};
    wgpuAdapterRequestDevice(adapter, &deviceDesc, requestDeviceCb.Callback(),
                             requestDeviceCb.MakeUserdata(this));

    apiDevice = api.GetNewDevice();
    EXPECT_CALL(api, OnAdapterRequestDevice(apiAdapter, NotNull(), NotNull(), NotNull()))
        .WillOnce(Invoke(WithArg<1>([&](const auto* deviceDesc) {
            // Forward the callbacks to the mock callback storage.
            EXPECT_CALL(api, OnDeviceSetUncapturedErrorCallback(apiDevice, _, _));
            EXPECT_CALL(api, OnDeviceSetDeviceLostCallback(apiDevice, _, _));
            mockProcs.deviceSetUncapturedErrorCallback(apiDevice,
                                                       deviceDesc->uncapturedErrorCallback,
                                                       deviceDesc->uncapturedErrorUserdata);
            mockProcs.deviceSetDeviceLostCallback(apiDevice, deviceDesc->deviceLostCallback,
                                                  deviceDesc->deviceLostUserdata);

            EXPECT_CALL(api, DeviceGetLimits(apiDevice, NotNull()))
                .WillOnce(WithArg<1>(Invoke([&](WGPUSupportedLimits* limits) {
                    *limits = {};
                    return true;
                })));

            EXPECT_CALL(api, DeviceEnumerateFeatures(apiDevice, nullptr))
                .WillOnce(Return(0))
                .WillOnce(Return(0));

            api.CallAdapterRequestDeviceCallback(apiAdapter, WGPURequestDeviceStatus_Success,
                                                 apiDevice, nullptr);
        })));
    FlushClient();

    // Expect the callback in the client.
    EXPECT_CALL(requestDeviceCb, Call(WGPURequestDeviceStatus_Success, NotNull(), nullptr, this))
        .WillOnce(SaveArg<1>(&device));
    FlushServer();

    // auto deviceReservation = mWireClient->ReserveDevice();
    // EXPECT_CALL(api, DeviceReference(apiDevice));

    // device = deviceReservation.device;
    // apiDevice = api.GetNewDevice();

    // The GetQueue is done on WireClient startup so we expect it now.
    queue = wgpuDeviceGetQueue(device);
    apiQueue = api.GetNewQueue();
    EXPECT_CALL(api, DeviceGetQueue(apiDevice)).WillOnce(Return(apiQueue));
    FlushClient();
}

void WireTest::TearDown() {
    dawnProcSetProcs(nullptr);

    // Derived classes should call the base TearDown() first. The client must
    // be reset before any mocks are deleted.
    // Incomplete client callbacks will be called on deletion, so the mocks
    // cannot be null.
    api.IgnoreAllReleaseCalls();
    mWireClient = nullptr;
    mWireServer = nullptr;
}

// This should be called if |apiDevice| is no longer exists on the wire.
// This signals that expectations in |TearDown| shouldn't be added.
void WireTest::DefaultApiDeviceWasReleased() {
    apiDevice = nullptr;
}

void WireTest::FlushClient(bool success) {
    ASSERT_EQ(mC2sBuf->Flush(), success);

    Mock::VerifyAndClearExpectations(&api);
    SetupIgnoredCallExpectations();
}

void WireTest::FlushServer(bool success) {
    ASSERT_EQ(mS2cBuf->Flush(), success);
}

dawn::wire::WireServer* WireTest::GetWireServer() {
    return mWireServer.get();
}

dawn::wire::WireClient* WireTest::GetWireClient() {
    return mWireClient.get();
}

void WireTest::DeleteServer() {
    EXPECT_CALL(api, QueueRelease(apiQueue)).Times(1);
    EXPECT_CALL(api, DeviceRelease(apiDevice)).Times(1);
    EXPECT_CALL(api, AdapterRelease(apiAdapter)).Times(1);
    EXPECT_CALL(api, InstanceRelease(apiInstance)).Times(1);
    mWireServer = nullptr;
}

void WireTest::DeleteClient() {
    mWireClient = nullptr;
}

void WireTest::SetupIgnoredCallExpectations() {
    EXPECT_CALL(api, InstanceProcessEvents(_)).Times(AnyNumber());
    EXPECT_CALL(api, DeviceTick(_)).Times(AnyNumber());

    // The wire sets the device logging callback for forwarding it on device creation.
    // TODO(crbug.com/dawn/2279): Remove this after moving it to the device descriptor
    // with the other device callbacks.
    EXPECT_CALL(api, OnDeviceSetLoggingCallback(_, _, _)).Times(AnyNumber());
}
