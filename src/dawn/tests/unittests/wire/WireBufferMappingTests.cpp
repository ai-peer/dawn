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

#include <limits>
#include <memory>

#include "dawn/common/Assert.h"
#include "dawn/tests/unittests/wire/WireFutureTest.h"
#include "dawn/tests/unittests/wire/WireTest.h"
#include "dawn/wire/WireClient.h"

namespace dawn::wire {
namespace {

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

// Mock class to add expectations on the wire calling callbacks
class MockBufferMapCallback {
  public:
    MOCK_METHOD(void, Call, (WGPUBufferMapAsyncStatus status, void* userdata));
};

std::unique_ptr<StrictMock<MockBufferMapCallback>> mockBufferMapCallback;
void ToMockBufferMapCallback(WGPUBufferMapAsyncStatus status, void* userdata) {
    mockBufferMapCallback->Call(status, userdata);
}

struct MapModeImpl {
    // NOLINTNEXTLINE(runtime/explicit)
    MapModeImpl(WGPUMapMode mode) : mMode(mode) {}
    WGPUMapMode mMode;
};
std::ostream& operator<<(std::ostream& os, const MapModeImpl& param) {
    switch (param.mMode) {
        case WGPUMapMode_Read:
            os << "Read";
            break;
        case WGPUMapMode_Write:
            os << "Write";
            break;
        default:
            DAWN_UNREACHABLE();
    }
    return os;
}
using MapMode = std::optional<MapModeImpl>;
DAWN_WIRE_FUTURE_TEST_PARAM_STRUCT(WireBufferParam, MapMode);

using WireBufferMappingTestBase = WireFutureTestWithParams<WGPUBufferMapCallback,
                                                           WGPUBufferMapCallbackInfo,
                                                           wgpuBufferMapAsync,
                                                           wgpuBufferMapAsyncF,
                                                           WireBufferParam>;

// General mapping tests that either do not care about the specific mapping mode, or apply to both.
class WireBufferMappingTests : public WireBufferMappingTestBase {
  public:
    // Overriden version of wgpuBufferMapAsync that defers to the API call based on the
    // test callback mode.
    void BufferMapAsync(WGPUBuffer b,
                        WGPUMapMode mode,
                        size_t offset,
                        size_t size,
                        WGPUBufferMapCallback cb,
                        void* userdata) {
        CallImpl(cb, userdata, b, mode, offset, size);
    }

    WGPUMapMode GetMapMode() {
        DAWN_ASSERT(GetParam().mMapMode);
        return (*GetParam().mMapMode).mMode;
    }

  protected:
    void SetUp() override {
        WireBufferMappingTestBase::SetUp();

        mockBufferMapCallback = std::make_unique<StrictMock<MockBufferMapCallback>>();
        apiBuffer = api.GetNewBuffer();
    }

    void TearDown() override {
        WireBufferMappingTestBase::TearDown();

        // Delete mock so that expectations are checked
        mockBufferMapCallback = nullptr;
    }

    void FlushServer() {
        WireTest::FlushServer();
        Mock::VerifyAndClearExpectations(&mockBufferMapCallback);
    }

    void FlushServerFutures() {
        WireBufferMappingTestBase::FlushServerFutures();
        ASSERT_TRUE(Mock::VerifyAndClearExpectations(mockBufferMapCallback.get()));
    }

    void SetupBuffer(WGPUMapMode mapMode) {
        WGPUBufferUsageFlags usage = WGPUBufferUsage_MapRead;
        if (mapMode == WGPUMapMode_Read) {
            usage = WGPUBufferUsage_MapRead;
        } else if (mapMode == WGPUMapMode_Write) {
            usage = WGPUBufferUsage_MapWrite;
        }

        WGPUBufferDescriptor descriptor = {};
        descriptor.size = kBufferSize;
        descriptor.usage = usage;

        buffer = wgpuDeviceCreateBuffer(device, &descriptor);

        EXPECT_CALL(api, DeviceCreateBuffer(apiDevice, _))
            .WillOnce(Return(apiBuffer))
            .RetiresOnSaturation();
        FlushClient();
    }

    // Sets up the correct mapped range call expectations given the map mode.
    void ExpectMappedRangeCall(uint64_t bufferSize, void* bufferContent) {
        WGPUMapMode mapMode = GetMapMode();
        if (mapMode == WGPUMapMode_Read) {
            EXPECT_CALL(api, BufferGetConstMappedRange(apiBuffer, 0, bufferSize))
                .WillOnce(Return(bufferContent));
        } else if (mapMode == WGPUMapMode_Write) {
            EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, bufferSize))
                .WillOnce(Return(bufferContent));
        }
    }

    static constexpr uint64_t kBufferSize = sizeof(uint32_t);
    // A successfully created buffer
    WGPUBuffer buffer;
    WGPUBuffer apiBuffer;
};

DAWN_INSTANTIATE_WIRE_FUTURE_TEST_P(WireBufferMappingTests, {WGPUMapMode_Read, WGPUMapMode_Write});

// Check that things work correctly when a validation error happens when mapping the buffer.
TEST_P(WireBufferMappingTests, ErrorWhileMapping) {
    WGPUMapMode mapMode = GetMapMode();
    SetupBuffer(mapMode);
    BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, mapMode, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&] {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_ValidationError);
        }));

    FlushClientFutures();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_ValidationError, _)).Times(1);

    FlushServerFutures();

    EXPECT_EQ(nullptr, wgpuBufferGetConstMappedRange(buffer, 0, kBufferSize));
}

// Check that the map read callback is called with "DestroyedBeforeCallback" when the buffer is
// destroyed before the request is finished
TEST_P(WireBufferMappingTests, DestroyBeforeRequestEnd) {
    WGPUMapMode mapMode = GetMapMode();
    SetupBuffer(mapMode);
    BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    // Return success
    uint32_t bufferContent = 0;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, mapMode, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&] { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success); }));
    ExpectMappedRangeCall(kBufferSize, &bufferContent);

    // Destroy before the client gets the success, so the callback is called with
    // DestroyedBeforeCallback.
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_DestroyedBeforeCallback, _))
        .Times(1);

    wgpuBufferRelease(buffer);
    EXPECT_CALL(api, BufferRelease(apiBuffer));

    FlushClientFutures();
}

// Check the map callback is called with "UnmappedBeforeCallback" when the map request would have
// worked, but Unmap was called.
TEST_P(WireBufferMappingTests, UnmapCalledTooEarly) {
    WGPUMapMode mapMode = GetMapMode();
    SetupBuffer(mapMode);
    BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, mapMode, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&] { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success); }));
    ExpectMappedRangeCall(kBufferSize, &bufferContent);

    // The callback should get called immediately with UnmappedBeforeCallback status
    // even if the request succeeds on the server side
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_UnmappedBeforeCallback, _))
        .Times(1);

    // Oh no! We are calling Unmap too early! The callback should get fired immediately
    // before we get an answer from the server.
    wgpuBufferUnmap(buffer);
    EXPECT_CALL(api, BufferUnmap(apiBuffer));

    FlushClientFutures();
}

// Check that if Unmap() was called early client-side, we disregard server-side validation errors.
TEST_P(WireBufferMappingTests, UnmapCalledTooEarlyServerSideError) {
    WGPUMapMode mapMode = GetMapMode();
    SetupBuffer(mapMode);
    BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, mapMode, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&] {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_ValidationError);
        }));

    // The callback should get called immediately with UnmappedBeforeCallback status,
    // not server-side error, even if the request fails on the server side
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_UnmappedBeforeCallback, _))
        .Times(1);

    // Oh no! We are calling Unmap too early! The callback should get fired immediately
    // before we get an answer from the server that the mapAsync call was an error.
    wgpuBufferUnmap(buffer);
    EXPECT_CALL(api, BufferUnmap(apiBuffer));

    FlushClientFutures();
}

// Check the map callback is called with "DestroyedBeforeCallback" when the map request would have
// worked, but Destroy was called
TEST_P(WireBufferMappingTests, DestroyCalledTooEarly) {
    WGPUMapMode mapMode = GetMapMode();
    SetupBuffer(mapMode);
    BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, mapMode, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&] { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success); }));
    ExpectMappedRangeCall(kBufferSize, &bufferContent);

    // The callback should get called immediately with DestroyedBeforeCallback status
    // even if the request succeeds on the server side
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_DestroyedBeforeCallback, _))
        .Times(1);

    // Oh no! We are calling Destroy too early! The callback should get fired immediately
    // before we get an answer from the server.
    wgpuBufferDestroy(buffer);
    EXPECT_CALL(api, BufferDestroy(apiBuffer));

    FlushClientFutures();
}

// Check that if Destroy() was called early client-side, we disregard server-side validation errors.
TEST_P(WireBufferMappingTests, DestroyCalledTooEarlyServerSideError) {
    WGPUMapMode mapMode = GetMapMode();
    SetupBuffer(mapMode);
    BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, mapMode, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&] {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_ValidationError);
        }));

    // The callback should get called immediately with UnmappedBeforeCallback status,
    // not server-side error, even if the request fails on the server side
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_DestroyedBeforeCallback, _))
        .Times(1);

    // Oh no! We are calling Destroy too early! The callback should get fired immediately
    // before we get an answer from the server that the mapAsync call was an error.
    wgpuBufferDestroy(buffer);
    EXPECT_CALL(api, BufferDestroy(apiBuffer));

    FlushClientFutures();
}

// Test that the MapReadCallback isn't fired twice when unmap() is called inside the callback
TEST_P(WireBufferMappingTests, UnmapInsideMapCallback) {
    WGPUMapMode mapMode = GetMapMode();
    SetupBuffer(mapMode);
    BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, mapMode, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&] { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success); }));
    ExpectMappedRangeCall(kBufferSize, &bufferContent);

    FlushClientFutures();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _))
        .WillOnce(InvokeWithoutArgs([&] { wgpuBufferUnmap(buffer); }));

    FlushServerFutures();

    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();
}

// Test that the MapReadCallback isn't fired twice the buffer external refcount reaches 0 in the
// callback
TEST_P(WireBufferMappingTests, DestroyInsideMapCallback) {
    WGPUMapMode mapMode = GetMapMode();
    SetupBuffer(mapMode);
    BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, mapMode, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&] { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success); }));
    ExpectMappedRangeCall(kBufferSize, &bufferContent);

    FlushClientFutures();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _))
        .WillOnce(InvokeWithoutArgs([&] { wgpuBufferRelease(buffer); }));

    FlushServerFutures();

    EXPECT_CALL(api, BufferRelease(apiBuffer));

    FlushClient();
}

// Tests specific to mapping for reading
class WireBufferMappingReadTests : public WireBufferMappingTests {
  public:
    WireBufferMappingReadTests() {}
    ~WireBufferMappingReadTests() override = default;

    void SetUp() override {
        WireBufferMappingTests::SetUp();

        SetupBuffer(WGPUMapMode_Read);
    }
};

DAWN_INSTANTIATE_WIRE_FUTURE_TEST_P(WireBufferMappingReadTests);

// Check mapping for reading a succesfully created buffer
TEST_P(WireBufferMappingReadTests, MappingSuccess) {
    BufferMapAsync(buffer, WGPUMapMode_Read, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Read, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&] { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success); }));
    EXPECT_CALL(api, BufferGetConstMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    FlushClientFutures();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _)).Times(1);

    FlushServerFutures();

    EXPECT_EQ(bufferContent,
              *static_cast<const uint32_t*>(wgpuBufferGetConstMappedRange(buffer, 0, kBufferSize)));

    wgpuBufferUnmap(buffer);
    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();
}

// Check that an error map read while a buffer is already mapped won't changed the result of get
// mapped range
TEST_P(WireBufferMappingReadTests, MappingErrorWhileAlreadyMapped) {
    // Successful map
    BufferMapAsync(buffer, WGPUMapMode_Read, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Read, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&] { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success); }));
    EXPECT_CALL(api, BufferGetConstMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    FlushClientFutures();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _)).Times(1);

    FlushServerFutures();

    // Map failure while the buffer is already mapped
    BufferMapAsync(buffer, WGPUMapMode_Read, 0, kBufferSize, ToMockBufferMapCallback, nullptr);
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Read, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&] {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_ValidationError);
        }));

    FlushClientFutures();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_ValidationError, _)).Times(1);

    FlushServerFutures();

    EXPECT_EQ(bufferContent,
              *static_cast<const uint32_t*>(wgpuBufferGetConstMappedRange(buffer, 0, kBufferSize)));
}

// Tests specific to mapping for writing
class WireBufferMappingWriteTests : public WireBufferMappingTests {
  public:
    WireBufferMappingWriteTests() {}
    ~WireBufferMappingWriteTests() override = default;

    void SetUp() override {
        WireBufferMappingTests::SetUp();

        SetupBuffer(WGPUMapMode_Write);
    }
};

DAWN_INSTANTIATE_WIRE_FUTURE_TEST_P(WireBufferMappingWriteTests);

// Check mapping for writing a succesfully created buffer
TEST_P(WireBufferMappingWriteTests, MappingSuccess) {
    BufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t serverBufferContent = 31337;
    uint32_t updatedContent = 4242;

    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&] { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success); }));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&serverBufferContent));

    FlushClientFutures();

    // The map write callback always gets a buffer full of zeroes.
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _)).Times(1);

    FlushServerFutures();

    uint32_t* lastMapWritePointer =
        static_cast<uint32_t*>(wgpuBufferGetMappedRange(buffer, 0, kBufferSize));
    ASSERT_EQ(0u, *lastMapWritePointer);

    // Write something to the mapped pointer
    *lastMapWritePointer = updatedContent;

    wgpuBufferUnmap(buffer);
    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();

    // After the buffer is unmapped, the content of the buffer is updated on the server
    ASSERT_EQ(serverBufferContent, updatedContent);
}

// Check that an error map write while a buffer is already mapped
TEST_P(WireBufferMappingWriteTests, MappingErrorWhileAlreadyMapped) {
    // Successful map
    BufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&] { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success); }));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    FlushClientFutures();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _)).Times(1);

    FlushServerFutures();

    // Map failure while the buffer is already mapped
    BufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&] {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_ValidationError);
        }));

    FlushClientFutures();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_ValidationError, _)).Times(1);

    FlushServerFutures();

    EXPECT_NE(nullptr,
              static_cast<const uint32_t*>(wgpuBufferGetConstMappedRange(buffer, 0, kBufferSize)));
}

// Test that the MapWriteCallback isn't fired twice the buffer external refcount reaches 0 in
// the callback
// TODO(dawn:1621): Suppressed because the mapping handling still touches the buffer after it is
// destroyed triggering an ASAN error.
TEST_F(WireBufferMappingWriteTests, DISABLED_DestroyInsideMapWriteCallback) {
    wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 31337;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&] { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success); }));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&bufferContent));

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _))
        .WillOnce(InvokeWithoutArgs([&] { wgpuBufferRelease(buffer); }));

    FlushServer();

    EXPECT_CALL(api, BufferRelease(apiBuffer));

    FlushClient();
}

// Tests specific to mapped at creation.
class WireBufferMappedAtCreationTests : public WireBufferMappingTests {};

DAWN_INSTANTIATE_WIRE_FUTURE_TEST_P(WireBufferMappedAtCreationTests);

// Test successful buffer creation with mappedAtCreation=true
TEST_F(WireBufferMappedAtCreationTests, Success) {
    WGPUBufferDescriptor descriptor = {};
    descriptor.size = 4;
    descriptor.mappedAtCreation = true;

    WGPUBuffer apiBuffer = api.GetNewBuffer();
    uint32_t apiBufferData = 1234;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &descriptor);

    EXPECT_CALL(api, DeviceCreateBuffer(apiDevice, _)).WillOnce(Return(apiBuffer));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, 4)).WillOnce(Return(&apiBufferData));

    FlushClient();

    wgpuBufferUnmap(buffer);
    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();
}

// Test that releasing a buffer mapped at creation does not call Unmap
TEST_F(WireBufferMappedAtCreationTests, ReleaseBeforeUnmap) {
    WGPUBufferDescriptor descriptor = {};
    descriptor.size = 4;
    descriptor.mappedAtCreation = true;

    WGPUBuffer apiBuffer = api.GetNewBuffer();
    uint32_t apiBufferData = 1234;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &descriptor);

    EXPECT_CALL(api, DeviceCreateBuffer(apiDevice, _)).WillOnce(Return(apiBuffer));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, 4)).WillOnce(Return(&apiBufferData));

    FlushClient();

    wgpuBufferRelease(buffer);
    EXPECT_CALL(api, BufferRelease(apiBuffer)).Times(1);

    FlushClient();
}

// Test that it is valid to map a buffer after it is mapped at creation and unmapped
TEST_P(WireBufferMappedAtCreationTests, MapSuccess) {
    WGPUBufferDescriptor descriptor = {};
    descriptor.size = 4;
    descriptor.usage = WGPUMapMode_Write;
    descriptor.mappedAtCreation = true;

    WGPUBuffer apiBuffer = api.GetNewBuffer();
    uint32_t apiBufferData = 1234;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &descriptor);

    EXPECT_CALL(api, DeviceCreateBuffer(apiDevice, _)).WillOnce(Return(apiBuffer));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, 4)).WillOnce(Return(&apiBufferData));

    FlushClient();

    wgpuBufferUnmap(buffer);
    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();

    BufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&] { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success); }));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, kBufferSize))
        .WillOnce(Return(&apiBufferData));

    FlushClientFutures();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _)).Times(1);

    FlushServerFutures();
}

// Test that it is invalid to map a buffer after mappedAtCreation but before Unmap
TEST_P(WireBufferMappedAtCreationTests, MapFailure) {
    WGPUBufferDescriptor descriptor = {};
    descriptor.size = 4;
    descriptor.mappedAtCreation = true;

    WGPUBuffer apiBuffer = api.GetNewBuffer();
    uint32_t apiBufferData = 1234;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &descriptor);

    EXPECT_CALL(api, DeviceCreateBuffer(apiDevice, _)).WillOnce(Return(apiBuffer));
    EXPECT_CALL(api, BufferGetMappedRange(apiBuffer, 0, 4)).WillOnce(Return(&apiBufferData));

    FlushClient();

    BufferMapAsync(buffer, WGPUMapMode_Write, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    // Note that the validation logic is entirely on the native side so we inject the validation
    // error here and flush the server response to mock the expected behavior.
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, WGPUMapMode_Write, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs([&] {
            api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_ValidationError);
        }));

    FlushClientFutures();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_ValidationError, _)).Times(1);

    FlushServerFutures();

    EXPECT_NE(nullptr,
              static_cast<const uint32_t*>(wgpuBufferGetConstMappedRange(buffer, 0, kBufferSize)));

    wgpuBufferUnmap(buffer);
    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();
}

// Check that trying to create a buffer of size MAX_SIZE_T won't get OOM error at the client side.
TEST_F(WireBufferMappingTests, MaxSizeMappableBufferOOMDirectly) {
    size_t kOOMSize = std::numeric_limits<size_t>::max();
    WGPUBuffer apiBuffer = api.GetNewBuffer();

    // Check for CreateBufferMapped.
    {
        WGPUBufferDescriptor descriptor = {};
        descriptor.usage = WGPUBufferUsage_CopySrc;
        descriptor.size = kOOMSize;
        descriptor.mappedAtCreation = true;

        wgpuDeviceCreateBuffer(device, &descriptor);
        FlushClient();
    }

    // Check for MapRead usage.
    {
        WGPUBufferDescriptor descriptor = {};
        descriptor.usage = WGPUBufferUsage_MapRead;
        descriptor.size = kOOMSize;

        wgpuDeviceCreateBuffer(device, &descriptor);
        EXPECT_CALL(api, DeviceCreateErrorBuffer(apiDevice, _)).WillOnce(Return(apiBuffer));
        FlushClient();
    }

    // Check for MapWrite usage.
    {
        WGPUBufferDescriptor descriptor = {};
        descriptor.usage = WGPUBufferUsage_MapWrite;
        descriptor.size = kOOMSize;

        wgpuDeviceCreateBuffer(device, &descriptor);
        EXPECT_CALL(api, DeviceCreateErrorBuffer(apiDevice, _)).WillOnce(Return(apiBuffer));
        FlushClient();
    }
}

// Test that registering a callback then wire disconnect calls the callback with
// DeviceLost.
TEST_P(WireBufferMappingTests, MapThenDisconnect) {
    WGPUMapMode mapMode = GetMapMode();
    SetupBuffer(mapMode);
    BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    uint32_t bufferContent = 0;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, mapMode, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&] { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success); }));
    ExpectMappedRangeCall(kBufferSize, &bufferContent);

    // Only flush the client, not the client futures, otherwise for WaitAny or ProcessEvent modes,
    // the server would have responded and the device lost would be masked.
    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_DeviceLost, _)).Times(1);
    GetWireClient()->Disconnect();
}

// Test that registering a callback after wire disconnect calls the callback with
// DeviceLost.
TEST_P(WireBufferMappingTests, MapAfterDisconnect) {
    WGPUMapMode mapMode = GetMapMode();
    SetupBuffer(mapMode);

    GetWireClient()->Disconnect();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_DeviceLost, _)).Times(1);
    BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallback, nullptr);
}

// Test that mapping again while pending map cause an error on the callback.
TEST_P(WireBufferMappingTests, PendingMapImmediateError) {
    WGPUMapMode mapMode = GetMapMode();
    SetupBuffer(mapMode);
    BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    // Calls for the first successful map.
    uint32_t bufferContent = 0;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, mapMode, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&] { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success); }));
    ExpectMappedRangeCall(kBufferSize, &bufferContent);

    // In spontaneous mode, this callback fires as soon as we make the call.
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_MappingAlreadyPending, _))
        .Times(1);
    BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

    FlushClientFutures();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _)).Times(1);

    FlushServerFutures();
}

// Test that GetMapState() returns map state as expected
TEST_P(WireBufferMappingTests, GetMapState) {
    WGPUMapMode mapMode = GetMapMode();
    SetupBuffer(mapMode);

    // Server-side success case
    {
        uint32_t bufferContent = 31337;
        EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, mapMode, 0, kBufferSize, _, _))
            .WillOnce(InvokeWithoutArgs([&] {
                api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success);
            }));
        ExpectMappedRangeCall(kBufferSize, &bufferContent);
        EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, _)).Times(1);

        ASSERT_EQ(wgpuBufferGetMapState(buffer), WGPUBufferMapState_Unmapped);
        BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

        // map state should become pending immediately after map async call
        ASSERT_EQ(wgpuBufferGetMapState(buffer), WGPUBufferMapState_Pending);
        FlushClient();

        // map state should be pending until receiving a response from server
        ASSERT_EQ(wgpuBufferGetMapState(buffer), WGPUBufferMapState_Pending);
        FlushServerFutures();

        // mapping succeeded
        ASSERT_EQ(wgpuBufferGetMapState(buffer), WGPUBufferMapState_Mapped);
    }

    wgpuBufferUnmap(buffer);
    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);
    FlushClient();

    // Server-side error case
    {
        EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, mapMode, 0, kBufferSize, _, _))
            .WillOnce(InvokeWithoutArgs([&] {
                api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_ValidationError);
            }));
        EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_ValidationError, _))
            .Times(1);

        ASSERT_EQ(wgpuBufferGetMapState(buffer), WGPUBufferMapState_Unmapped);
        BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallback, nullptr);

        // map state should become pending immediately after map async call
        ASSERT_EQ(wgpuBufferGetMapState(buffer), WGPUBufferMapState_Pending);
        FlushClient();

        // map state should be pending until receiving a response from server
        ASSERT_EQ(wgpuBufferGetMapState(buffer), WGPUBufferMapState_Pending);
        FlushServerFutures();

        // mapping failed
        ASSERT_EQ(wgpuBufferGetMapState(buffer), WGPUBufferMapState_Unmapped);
    }
}

// Hack to pass in test context into user callback
struct TestData {
    WireBufferMappingTests* pTest;
    WGPUBuffer* pTestBuffer;
    size_t numRequests;
};

#if defined(DAWN_ENABLE_ASSERTS)
static void ToMockBufferMapCallbackWithAssertErrorRequest(WGPUBufferMapAsyncStatus status,
                                                          void* userdata) {
    TestData* testData = reinterpret_cast<TestData*>(userdata);
    mockBufferMapCallback->Call(status, testData->pTestBuffer);
    ASSERT_DEATH_IF_SUPPORTED(
        {
            // This map async should cause assertion error because of
            // refcount == 0.
            testData->pTest->BufferMapAsync(*testData->pTestBuffer, testData->pTest->GetMapMode(),
                                            0, sizeof(uint32_t), ToMockBufferMapCallback, nullptr);
        },
        "");
}

// Test that request inside user callbacks after object destruction is called
TEST_P(WireBufferMappingTests, MapInsideCallbackAfterDestruction) {
    WGPUMapMode mapMode = GetMapMode();
    SetupBuffer(mapMode);
    TestData testData = {this, &buffer, 0};
    BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallbackWithAssertErrorRequest,
                   &testData);

    // By releasing the buffer the refcount reaches zero and pending map async
    // should fail with destroyed before callback status.
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_DestroyedBeforeCallback, _))
        .Times(1);
    wgpuBufferRelease(buffer);
}
#endif  // defined(DAWN_ENABLE_ASSERTS)

static void ToMockBufferMapCallbackWithNewRequests(WGPUBufferMapAsyncStatus status,
                                                   void* userdata) {
    TestData* testData = reinterpret_cast<TestData*>(userdata);
    // Mimic the user callback is sending new requests
    ASSERT_NE(testData, nullptr);
    ASSERT_NE(testData->pTest, nullptr);
    ASSERT_NE(testData->pTestBuffer, nullptr);

    mockBufferMapCallback->Call(status, testData->pTest);

    // Send the requests a number of times
    for (size_t i = 0; i < testData->numRequests; i++) {
        testData->pTest->BufferMapAsync(*(testData->pTestBuffer), testData->pTest->GetMapMode(), 0,
                                        sizeof(uint32_t), ToMockBufferMapCallback, testData->pTest);
    }
}

// Test that requests inside user callbacks before disconnect are called
TEST_P(WireBufferMappingTests, MapInsideCallbackBeforeDisconnect) {
    WGPUMapMode mapMode = GetMapMode();
    SetupBuffer(mapMode);
    TestData testData = {this, &buffer, 10};
    BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallbackWithNewRequests,
                   &testData);

    uint32_t bufferContent = 0;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, mapMode, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&] { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success); }));
    ExpectMappedRangeCall(kBufferSize, &bufferContent);

    FlushClient();

    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_DeviceLost, this))
        .Times(testData.numRequests + 1);
    GetWireClient()->Disconnect();
}

// Test that requests inside user callbacks before object destruction are called
TEST_P(WireBufferMappingTests, MapInsideCallbackBeforeDestruction) {
    WGPUMapMode mapMode = GetMapMode();
    SetupBuffer(mapMode);
    TestData testData = {this, &buffer, 10};
    BufferMapAsync(buffer, mapMode, 0, kBufferSize, ToMockBufferMapCallbackWithNewRequests,
                   &testData);

    uint32_t bufferContent = 0;
    EXPECT_CALL(api, OnBufferMapAsync(apiBuffer, mapMode, 0, kBufferSize, _, _))
        .WillOnce(InvokeWithoutArgs(
            [&] { api.CallBufferMapAsyncCallback(apiBuffer, WGPUBufferMapAsyncStatus_Success); }));
    ExpectMappedRangeCall(kBufferSize, &bufferContent);

    FlushClient();

    // The first map async call should succeed
    EXPECT_CALL(*mockBufferMapCallback, Call(WGPUBufferMapAsyncStatus_Success, this)).Times(1);

    // For the legacy and Spontaneous callback modes, flushing the server will immediately call
    // all of the callbacks accordingly, whereas in WaitOnly and ProcessEvents mode we need to
    // synchronize.
    CallbackMode callbackMode = GetCallbackMode();
    bool spontaneous =
        callbackMode == CallbackMode::Async || callbackMode == CallbackMode::Spontaneous;
    auto SetExpectations = [&]() {
        // The second or later map async calls in the map async callback
        // should immediately fail because of pending map
        EXPECT_CALL(*mockBufferMapCallback,
                    Call(WGPUBufferMapAsyncStatus_MappingAlreadyPending, this))
            .Times(testData.numRequests - 1);

        // The first map async call in the map async callback should fail
        // with destroyed before callback status due to buffer release below
        EXPECT_CALL(*mockBufferMapCallback,
                    Call(WGPUBufferMapAsyncStatus_DestroyedBeforeCallback, this))
            .Times(1);
    };
    if (spontaneous) {
        // All expectations will occur immediately on flush.
        SetExpectations();
        FlushServer();
        wgpuBufferRelease(buffer);
    } else {
        // First flush will only trigger the callback for the success. Other callbacks are just
        // being queued.
        FlushServerFutures();

        SetExpectations();
        wgpuBufferRelease(buffer);
        FlushServerFutures();
    }
}

}  // anonymous namespace
}  // namespace dawn::wire
