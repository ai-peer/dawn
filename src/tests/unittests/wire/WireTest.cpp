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

using namespace testing;
using namespace dawn_wire;

WireTest::WireTest() {
}

WireTest::~WireTest() {
}

client::MemoryTransferService* WireTest::GetClientMemoryTransferService() {
    return nullptr;
}

server::MemoryTransferService* WireTest::GetServerMemoryTransferService() {
    return nullptr;
}

void WireTest::SetUp() {
    DawnProcTable mockProcs;
    WGPUInstance mockInstance;
    api.GetProcTableAndInstance(&mockProcs, &mockInstance);

    SetupIgnoredCallExpectations();

    mS2cBuf = std::make_unique<utils::TerribleCommandBuffer>();
    mC2sBuf = std::make_unique<utils::TerribleCommandBuffer>(mWireServer.get());

    WireServerDescriptor serverDesc = {};
    serverDesc.instance = mockInstance;
    serverDesc.procs = &mockProcs;
    serverDesc.serializer = mS2cBuf.get();
    serverDesc.memoryTransferService = GetServerMemoryTransferService();

    mWireServer.reset(new WireServer(serverDesc));
    mC2sBuf->SetHandler(mWireServer.get());

    WireClientDescriptor clientDesc = {};
    clientDesc.serializer = mC2sBuf.get();
    clientDesc.memoryTransferService = GetClientMemoryTransferService();

    mWireClient.reset(new WireClient(clientDesc));
    mS2cBuf->SetHandler(mWireClient.get());

    instance = mWireClient->GetInstance();
    dawnProcSetProcs(&dawn_wire::client::GetProcs());

    apiInstance = mockInstance;

    WGPUAdapter adapter = nullptr;
    WGPURequestAdapterOptions options = {};
    wgpuInstanceRequestAdapter(
        instance, &options,
        [](WGPURequestAdapterStatus, WGPUAdapter adapter, void* userdata) {
            *reinterpret_cast<WGPUAdapter*>(userdata) = adapter;
        },
        &adapter);

    apiAdapter = api.GetNewAdapter();
    EXPECT_CALL(api, OnInstanceRequestAdapterCallback(apiInstance, _, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallInstanceRequestAdapterCallback(apiInstance, WGPURequestAdapterStatus_Success,
                                                   apiAdapter);
        }));

    FlushClient();
    FlushServer();
    ASSERT(adapter != nullptr);

    WGPUDeviceDescriptor deviceDescriptor = {};
    wgpuAdapterRequestDevice(
        adapter, &deviceDescriptor,
        [](WGPURequestDeviceStatus, WGPUDevice device, void* userdata) {
            *reinterpret_cast<WGPUDevice*>(userdata) = device;
        },
        &device);

    apiDevice = api.GetNewDevice();
    EXPECT_CALL(api, OnAdapterRequestDeviceCallback(apiAdapter, _, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallAdapterRequestDeviceCallback(apiAdapter, WGPURequestDeviceStatus_Success,
                                                 apiDevice);
        }));
    EXPECT_CALL(api, OnDeviceSetUncapturedErrorCallbackCallback(apiDevice, _, _)).Times(Exactly(1));
    EXPECT_CALL(api, OnDeviceSetDeviceLostCallbackCallback(apiDevice, _, _)).Times(Exactly(1));

    FlushClient();
    FlushServer();
    ASSERT(device != nullptr);

    queue = wgpuDeviceGetDefaultQueue(device);
    apiQueue = api.GetNewQueue();
    EXPECT_CALL(api, DeviceGetDefaultQueue(apiDevice)).WillOnce(Return(apiQueue));
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

void WireTest::FlushClient(bool success) {
    ASSERT_EQ(mC2sBuf->Flush(), success);

    Mock::VerifyAndClearExpectations(&api);
    SetupIgnoredCallExpectations();
}

void WireTest::FlushServer(bool success) {
    ASSERT_EQ(mS2cBuf->Flush(), success);
}

dawn_wire::WireServer* WireTest::GetWireServer() {
    return mWireServer.get();
}

dawn_wire::WireClient* WireTest::GetWireClient() {
    return mWireClient.get();
}

void WireTest::DeleteServer() {
    EXPECT_CALL(api, AdapterRelease(apiAdapter)).Times(1);
    EXPECT_CALL(api, DeviceRelease(apiDevice)).Times(1);
    EXPECT_CALL(api, QueueRelease(apiQueue)).Times(1);
    mWireServer = nullptr;
}

void WireTest::DeleteClient() {
    mWireClient = nullptr;
}

void WireTest::SetupIgnoredCallExpectations() {
    EXPECT_CALL(api, DeviceTick(_)).Times(AnyNumber());
}
