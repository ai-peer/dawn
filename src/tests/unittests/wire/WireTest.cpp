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

#include "dawn_wire/WireClient.h"
#include "dawn_wire/WireServer.h"
#include "utils/TerribleCommandBuffer.h"

using namespace testing;
using namespace dawn_wire;

std::unique_ptr<MockDeviceErrorCallback> WireTest::mockDeviceErrorCallback = nullptr;
std::unique_ptr<MockBuilderErrorCallback> WireTest::mockBuilderErrorCallback = nullptr;
std::unique_ptr<MockBufferMapReadCallback> WireTest::mockBufferMapReadCallback = nullptr;
std::unique_ptr<MockBufferMapWriteCallback> WireTest::mockBufferMapWriteCallback = nullptr;
uint32_t* WireTest::lastMapWritePointer = nullptr;
std::unique_ptr<MockCreateBufferMappedCallback> WireTest::mockCreateBufferMappedCallback = nullptr;
dawnBuffer WireTest::lastCreateMappedBuffer = nullptr;
std::unique_ptr<MockFenceOnCompletionCallback> WireTest::mockFenceOnCompletionCallback = nullptr;

void WireTest::ToMockDeviceErrorCallback(const char* message, dawnCallbackUserdata userdata) {
    mockDeviceErrorCallback->Call(message, userdata);
}

void WireTest::ToMockBuilderErrorCallback(dawnBuilderErrorStatus status,
                                          const char* message,
                                          dawnCallbackUserdata userdata1,
                                          dawnCallbackUserdata userdata2) {
    mockBuilderErrorCallback->Call(status, message, userdata1, userdata2);
}

void WireTest::ToMockBufferMapReadCallback(dawnBufferMapAsyncStatus status,
                                           const void* ptr,
                                           uint32_t dataLength,
                                           dawnCallbackUserdata userdata) {
    // Assume the data is uint32_t to make writing matchers easier
    mockBufferMapReadCallback->Call(status, static_cast<const uint32_t*>(ptr), dataLength,
                                    userdata);
}

void WireTest::ToMockBufferMapWriteCallback(dawnBufferMapAsyncStatus status,
                                            void* ptr,
                                            uint32_t dataLength,
                                            dawnCallbackUserdata userdata) {
    // Assume the data is uint32_t to make writing matchers easier
    lastMapWritePointer = static_cast<uint32_t*>(ptr);
    mockBufferMapWriteCallback->Call(status, lastMapWritePointer, dataLength, userdata);
}

void WireTest::ToMockCreateBufferMappedCallback(dawnBuffer buffer,
                                                dawnBufferMapAsyncStatus status,
                                                void* ptr,
                                                uint32_t dataLength,
                                                dawnCallbackUserdata userdata) {
    lastCreateMappedBuffer = buffer;
    lastMapWritePointer = static_cast<uint32_t*>(ptr);
    mockCreateBufferMappedCallback->Call(buffer, status, static_cast<uint32_t*>(ptr), dataLength,
                                         userdata);
}

void WireTest::ToMockFenceOnCompletionCallback(dawnFenceCompletionStatus status,
                                               dawnCallbackUserdata userdata) {
    mockFenceOnCompletionCallback->Call(status, userdata);
}

WireTest::WireTest(bool ignoreSetCallbackCalls) : mIgnoreSetCallbackCalls(ignoreSetCallbackCalls) {
}

WireTest::~WireTest() {
}

void WireTest::SetUp() {
    mockDeviceErrorCallback = std::make_unique<MockDeviceErrorCallback>();
    mockBuilderErrorCallback = std::make_unique<MockBuilderErrorCallback>();
    mockBufferMapReadCallback = std::make_unique<MockBufferMapReadCallback>();
    mockBufferMapWriteCallback = std::make_unique<MockBufferMapWriteCallback>();
    mockCreateBufferMappedCallback = std::make_unique<MockCreateBufferMappedCallback>();
    mockFenceOnCompletionCallback = std::make_unique<MockFenceOnCompletionCallback>();

    dawnProcTable mockProcs;
    dawnDevice mockDevice;
    api.GetProcTableAndDevice(&mockProcs, &mockDevice);

    // This SetCallback call cannot be ignored because it is done as soon as we start the server
    EXPECT_CALL(api, OnDeviceSetErrorCallback(_, _, _)).Times(Exactly(1));
    if (mIgnoreSetCallbackCalls) {
        EXPECT_CALL(api, OnBuilderSetErrorCallback(_, _, _, _)).Times(AnyNumber());
    }
    EXPECT_CALL(api, DeviceTick(_)).Times(AnyNumber());

    mS2cBuf = std::make_unique<utils::TerribleCommandBuffer>();
    mC2sBuf = std::make_unique<utils::TerribleCommandBuffer>(mWireServer.get());

    mWireServer.reset(new WireServer(mockDevice, mockProcs, mS2cBuf.get()));
    mC2sBuf->SetHandler(mWireServer.get());

    mWireClient.reset(new WireClient(mC2sBuf.get()));
    mS2cBuf->SetHandler(mWireClient.get());

    device = mWireClient->GetDevice();
    dawnProcTable clientProcs = mWireClient->GetProcs();
    dawnSetProcs(&clientProcs);

    apiDevice = mockDevice;
}

void WireTest::TearDown() {
    dawnSetProcs(nullptr);

    // Reset client before mocks are deleted.
    // Incomplete callbacks will be called on deletion, so the mocks cannot be null.
    mWireClient = nullptr;

    // Delete mocks so that expectations are checked
    mockDeviceErrorCallback = nullptr;
    mockBuilderErrorCallback = nullptr;
    mockBufferMapReadCallback = nullptr;
    mockBufferMapWriteCallback = nullptr;
    mockCreateBufferMappedCallback = nullptr;
    mockFenceOnCompletionCallback = nullptr;
}

void WireTest::FlushClient() {
    ASSERT_TRUE(mC2sBuf->Flush());
}

void WireTest::FlushServer() {
    ASSERT_TRUE(mS2cBuf->Flush());
}
