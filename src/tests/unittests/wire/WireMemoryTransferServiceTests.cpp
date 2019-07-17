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

#include "dawn_wire/client/ClientMemoryTransferService_mock.h"
#include "dawn_wire/server/ServerMemoryTransferService_mock.h"

using namespace testing;
using namespace dawn_wire;

// WireMemoryTransferServiceTests test the MemoryTransferService with buffer mapping.
// They test the basic success and error cases for buffer mapping, and they test
// mocked failures of each fallible MemoryTransferService method that an embedder
// could implement.
// The test harness defines multiple helpers for expecting operations on Read/Write handles
// and for mocking failures. The helpers are designed such that for a given run of a test,
// a Serialization expection has a corresponding Deserialization expectation for which the
// serialized data must match.
// There are tests which check for Success for every mapping operation which mock an entire mapping
// operation from map to unmap, and add all MemoryTransferService expectations.
// Tests which check for errors perform the same mapping operations but insert mocked failures for
// various mapping or MemoryTransferService operations.
class WireMemoryTransferServiceTests : public WireTest {
  public:
    WireMemoryTransferServiceTests() {
    }
    ~WireMemoryTransferServiceTests() override = default;

    client::MemoryTransferService* GetClientMemoryTransferService() override {
        return &clientMemoryTransferService;
    }

    server::MemoryTransferService* GetServerMemoryTransferService() override {
        return &serverMemoryTransferService;
    }

    void SetUp() override {
        WireTest::SetUp();

        // TODO(enga): Make this thread-safe.
        bufferContent++;
        mappedBufferContent = 0;
        updatedBufferContent++;
        serializeCreateInfo++;
        serializeInitialDataInfo++;
        serializeFlushInfo++;
    }

    void FlushClient(bool success = true) {
        WireTest::FlushClient(success);
        Mock::VerifyAndClearExpectations(&serverMemoryTransferService);
    }

    void FlushServer(bool success = true) {
        WireTest::FlushServer(success);
        Mock::VerifyAndClearExpectations(&clientMemoryTransferService);
    }

  protected:
    using ClientReadHandle = client::MockMemoryTransferService::MockReadHandle;
    using ServerReadHandle = server::MockMemoryTransferService::MockReadHandle;
    using ClientWriteHandle = client::MockMemoryTransferService::MockWriteHandle;
    using ServerWriteHandle = server::MockMemoryTransferService::MockWriteHandle;

    std::pair<DawnBuffer, DawnBuffer> CreateBuffer() {
        DawnBufferDescriptor descriptor;
        descriptor.nextInChain = nullptr;
        descriptor.size = sizeof(bufferContent);

        DawnBuffer apiBuffer = api.GetNewBuffer();
        DawnBuffer buffer = dawnDeviceCreateBuffer(device, &descriptor);

        EXPECT_CALL(api, DeviceCreateBuffer(apiDevice, _))
            .WillOnce(Return(apiBuffer))
            .RetiresOnSaturation();

        return std::make_pair(apiBuffer, buffer);
    }

    std::pair<DawnCreateBufferMappedResult, DawnCreateBufferMappedResult> CreateBufferMapped() {
        DawnBufferDescriptor descriptor;
        descriptor.nextInChain = nullptr;
        descriptor.size = sizeof(bufferContent);

        DawnBuffer apiBuffer = api.GetNewBuffer();

        DawnCreateBufferMappedResult apiResult;
        apiResult.buffer = apiBuffer;
        apiResult.data = reinterpret_cast<uint8_t*>(&mappedBufferContent);
        apiResult.dataLength = sizeof(mappedBufferContent);

        DawnCreateBufferMappedResult result = dawnDeviceCreateBufferMapped(device, &descriptor);

        EXPECT_CALL(api, DeviceCreateBufferMapped(apiDevice, _))
            .WillOnce(Return(apiResult))
            .RetiresOnSaturation();

        return std::make_pair(apiResult, result);
    }

    DawnCreateBufferMappedResult CreateBufferMappedAsync(DawnCreateBufferMappedResult* outResult) {
        // |outResult| is the client's result of createBufferMappedAsync. It is written when
        // the client's callback returns.

        DawnBufferDescriptor descriptor;
        descriptor.nextInChain = nullptr;
        descriptor.size = sizeof(bufferContent);

        dawnDeviceCreateBufferMappedAsync(
            device, &descriptor,
            [](DawnBufferMapAsyncStatus status, DawnCreateBufferMappedResult result,
               void* userdata) {
                if (status == DAWN_BUFFER_MAP_ASYNC_STATUS_UNKNOWN) {
                    // Early out if the status is UNKNOWN. This happens when the wire is
                    // destructed before the callback is received in tests which cause
                    // a fatal error in the wire.
                    return;
                }
                *reinterpret_cast<DawnCreateBufferMappedResult*>(userdata) = result;
            },
            outResult);

        DawnBuffer apiBuffer = api.GetNewBuffer();

        DawnCreateBufferMappedResult apiResult;
        apiResult.buffer = apiBuffer;
        apiResult.data = reinterpret_cast<uint8_t*>(&mappedBufferContent);
        apiResult.dataLength = sizeof(mappedBufferContent);

        EXPECT_CALL(api, DeviceCreateBufferMapped(apiDevice, _))
            .WillOnce(Return(apiResult))
            .RetiresOnSaturation();

        return apiResult;
    }

    ClientReadHandle* ExpectReadHandleCreation() {
        // Create the handle first so we can use it in later expectations.
        ClientReadHandle* handle = clientMemoryTransferService.NewReadHandle();

        EXPECT_CALL(clientMemoryTransferService, OnCreateReadHandle(sizeof(bufferContent)))
            .WillOnce(InvokeWithoutArgs([=]() {
                return handle;
            }));

        return handle;
    }

    void MockReadHandleCreationFailure() {
        EXPECT_CALL(clientMemoryTransferService, OnCreateReadHandle(sizeof(bufferContent)))
            .WillOnce(InvokeWithoutArgs([=]() { return nullptr; }));
    }

    void ExpectReadHandleSerialization(ClientReadHandle* handle) {
        EXPECT_CALL(clientMemoryTransferService, OnReadHandleSerializeCreate(handle, _))
            .WillOnce(InvokeWithoutArgs([&]() { return sizeof(serializeCreateInfo); }))
            .WillOnce(WithArg<1>([&](void* serializePointer) {
                memcpy(serializePointer, &serializeCreateInfo, sizeof(serializeCreateInfo));
                return sizeof(serializeCreateInfo);
            }));
    }

    ServerReadHandle* ExpectServerReadHandleDeserialize() {
        // Create the handle first so we can use it in later expectations.
        ServerReadHandle* handle = serverMemoryTransferService.NewReadHandle();

        EXPECT_CALL(serverMemoryTransferService,
                    OnDeserializeReadHandle(Pointee(Eq(serializeCreateInfo)),
                                            sizeof(serializeCreateInfo), _))
            .WillOnce(WithArg<2>([=](server::MemoryTransferService::ReadHandle** readHandle) {
                *readHandle = handle;
                return true;
            }));

        return handle;
    }

    void MockServerReadHandleDeserializeFailure() {
        EXPECT_CALL(serverMemoryTransferService,
                    OnDeserializeReadHandle(Pointee(Eq(serializeCreateInfo)),
                                            sizeof(serializeCreateInfo), _))
            .WillOnce(InvokeWithoutArgs([&]() { return false; }));
    }

    void ExpectServerReadHandleInitialize(ServerReadHandle* handle) {
        EXPECT_CALL(serverMemoryTransferService, OnReadHandleSerializeInitialData(handle, _, _, _))
            .WillOnce(InvokeWithoutArgs([&]() { return sizeof(serializeInitialDataInfo); }))
            .WillOnce(WithArg<3>([&](void* serializePointer) {
                memcpy(serializePointer, &serializeInitialDataInfo,
                       sizeof(serializeInitialDataInfo));
                return sizeof(serializeInitialDataInfo);
            }));
    }

    void ExpectClientReadHandleDeserializeInitialize(ClientReadHandle* handle,
                                                     uint32_t* mappedData) {
        EXPECT_CALL(clientMemoryTransferService, OnReadHandleDeserializeInitialData(
                                                     handle, Pointee(Eq(serializeInitialDataInfo)),
                                                     sizeof(serializeInitialDataInfo), _, _))
            .WillOnce(WithArgs<3, 4>([=](const void** data, size_t* dataLength) {
                *data = mappedData;
                *dataLength = sizeof(*mappedData);
                return true;
            }));
    }

    void MockClientReadHandleDeserializeInitializeFailure(ClientReadHandle* handle) {
        EXPECT_CALL(clientMemoryTransferService, OnReadHandleDeserializeInitialData(
                                                     handle, Pointee(Eq(serializeInitialDataInfo)),
                                                     sizeof(serializeInitialDataInfo), _, _))
            .WillOnce(InvokeWithoutArgs([&]() { return false; }));
    }

    ClientWriteHandle* ExpectWriteHandleCreation() {
        // Create the handle first so we can use it in later expectations.
        ClientWriteHandle* handle = clientMemoryTransferService.NewWriteHandle();

        EXPECT_CALL(clientMemoryTransferService, OnCreateWriteHandle(sizeof(bufferContent)))
            .WillOnce(InvokeWithoutArgs([=]() {
                return handle;
            }));

        return handle;
    }

    void MockWriteHandleCreationFailure() {
        EXPECT_CALL(clientMemoryTransferService, OnCreateWriteHandle(sizeof(bufferContent)))
            .WillOnce(InvokeWithoutArgs([=]() { return nullptr; }));
    }

    void ExpectWriteHandleSerialization(ClientWriteHandle* handle) {
        EXPECT_CALL(clientMemoryTransferService, OnWriteHandleSerializeCreate(handle, _))
            .WillOnce(InvokeWithoutArgs([&]() { return sizeof(serializeCreateInfo); }))
            .WillOnce(WithArg<1>([&](void* serializePointer) {
                memcpy(serializePointer, &serializeCreateInfo, sizeof(serializeCreateInfo));
                return sizeof(serializeCreateInfo);
            }));
    }

    ServerWriteHandle* ExpectServerWriteHandleDeserialization() {
        // Create the handle first so it can be used in later expectations.
        ServerWriteHandle* handle = serverMemoryTransferService.NewWriteHandle();

        EXPECT_CALL(serverMemoryTransferService,
                    OnDeserializeWriteHandle(Pointee(Eq(serializeCreateInfo)),
                                             sizeof(serializeCreateInfo), _))
            .WillOnce(WithArg<2>([=](server::MemoryTransferService::WriteHandle** writeHandle) {
                *writeHandle = handle;
                return true;
            }));

        return handle;
    }

    void MockServerWriteHandleDeserializeFailure() {
        EXPECT_CALL(serverMemoryTransferService,
                    OnDeserializeWriteHandle(Pointee(Eq(serializeCreateInfo)),
                                             sizeof(serializeCreateInfo), _))
            .WillOnce(InvokeWithoutArgs([&]() {
                return false;
            }));
    }

    void ExpectClientWriteHandleOpen(ClientWriteHandle* handle, uint32_t* mappedData) {
        EXPECT_CALL(clientMemoryTransferService, OnWriteHandleOpen(handle))
            .WillOnce(InvokeWithoutArgs([=]() {
                return std::make_pair(mappedData, sizeof(*mappedData));
            }));
    }

    void MockClientWriteHandleOpenFailure(ClientWriteHandle* handle) {
        EXPECT_CALL(clientMemoryTransferService, OnWriteHandleOpen(handle))
            .WillOnce(InvokeWithoutArgs(
                [&]() { return std::make_pair(nullptr, 0); }));
    }

    void ExpectClientWriteHandleSerializeFlush(ClientWriteHandle* handle) {
        EXPECT_CALL(clientMemoryTransferService, OnWriteHandleSerializeFlush(handle, _))
            .WillOnce(InvokeWithoutArgs([&]() { return sizeof(serializeFlushInfo); }))
            .WillOnce(WithArg<1>([&](void* serializePointer) {
                memcpy(serializePointer, &serializeFlushInfo, sizeof(serializeFlushInfo));
                return sizeof(serializeFlushInfo);
            }));
    }

    void ExpectServerWriteHandleDeserializeFlush(ServerWriteHandle* handle, uint32_t expectedData) {
        EXPECT_CALL(serverMemoryTransferService,
                    OnWriteHandleDeserializeFlush(handle, Pointee(Eq(serializeFlushInfo)),
                                                  sizeof(serializeFlushInfo)))
            .WillOnce(InvokeWithoutArgs([=]() {
                // The handle data should be updated.
                EXPECT_EQ(*handle->GetData(), expectedData);
                return true;
            }));
    }

    void MockServerWriteHandleDeserializeFlushFailure(ServerWriteHandle* handle) {
        EXPECT_CALL(serverMemoryTransferService,
                    OnWriteHandleDeserializeFlush(handle, Pointee(Eq(serializeFlushInfo)),
                                                  sizeof(serializeFlushInfo)))
            .WillOnce(InvokeWithoutArgs([&]() { return false; }));
    }

    // Arbitrary values used within tests to check if serialized data is correctly passed
    // between the client and server. The static data changes between runs of the tests and
    // test expectations will check that serialized values are passed to the respective deserialization
    // function.
    static uint32_t serializeCreateInfo;
    static uint32_t serializeInitialDataInfo;
    static uint32_t serializeFlushInfo;

    // Represents the buffer contents for the test.
    static uint32_t bufferContent;

    // The client's zero-initialized buffer for writing.
    uint32_t mappedBufferContent = 0;

    // |mappedBufferContent| should be set equal to |updatedBufferContent| when the client performs a write.
    // Test expectations should check that |bufferContent == updatedBufferContent| after all writes are flushed.
    static uint32_t updatedBufferContent;

    testing::StrictMock<dawn_wire::server::MockMemoryTransferService> serverMemoryTransferService;
    testing::StrictMock<dawn_wire::client::MockMemoryTransferService> clientMemoryTransferService;
};

uint32_t WireMemoryTransferServiceTests::bufferContent = 1337;
uint32_t WireMemoryTransferServiceTests::updatedBufferContent = 2349;
uint32_t WireMemoryTransferServiceTests::serializeCreateInfo = 4242;
uint32_t WireMemoryTransferServiceTests::serializeInitialDataInfo = 1394;
uint32_t WireMemoryTransferServiceTests::serializeFlushInfo = 1235;

// Test successful MapRead.
TEST_F(WireMemoryTransferServiceTests, BufferMapReadSuccess) {
    DawnBuffer buffer;
    DawnBuffer apiBuffer;
    std::tie(apiBuffer, buffer) = CreateBuffer();
    FlushClient();

    // The client should create and serialize a ReadHandle on mapReadAsync.
    ClientReadHandle* clientHandle = ExpectReadHandleCreation();
    ExpectReadHandleSerialization(clientHandle);

    dawnBufferMapReadAsync(buffer,
                           [](DawnBufferMapAsyncStatus status, const void* ptr, uint64_t dataLength,
                              void* userdata) {},
                           nullptr);

    // The server should deserialize the MapRead handle from the client and then serialize
    // an initialization message.
    ServerReadHandle* serverHandle = ExpectServerReadHandleDeserialize();
    ExpectServerReadHandleInitialize(serverHandle);

    // Mock a successful callback
    EXPECT_CALL(api, OnBufferMapReadAsyncCallback(apiBuffer, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallMapReadCallback(apiBuffer, DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS, &bufferContent,
                                    sizeof(bufferContent));
        }));

    FlushClient();

    // The client should receive the handle initialization message from the server.
    ExpectClientReadHandleDeserializeInitialize(clientHandle, &bufferContent);

    FlushServer();

    // The handle is destroyed once the buffer is unmapped.
    EXPECT_CALL(clientMemoryTransferService, OnReadHandleDestroy(clientHandle)).Times(1);
    dawnBufferUnmap(buffer);

    EXPECT_CALL(serverMemoryTransferService, OnReadHandleDestroy(serverHandle)).Times(1);
    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();
}

// Test unsuccessful MapRead.
TEST_F(WireMemoryTransferServiceTests, BufferMapReadError) {
    DawnBuffer buffer;
    DawnBuffer apiBuffer;
    std::tie(apiBuffer, buffer) = CreateBuffer();
    FlushClient();

    // The client should create and serialize a ReadHandle on mapReadAsync.
    ClientReadHandle* clientHandle = ExpectReadHandleCreation();
    ExpectReadHandleSerialization(clientHandle);

    dawnBufferMapReadAsync(buffer,
                           [](DawnBufferMapAsyncStatus status, const void* ptr, uint64_t dataLength,
                              void* userdata) {},
                           nullptr);

    // The server should deserialize the ReadHandle from the client.
    ServerReadHandle* serverHandle = ExpectServerReadHandleDeserialize();

    // Mock a failed callback.
    EXPECT_CALL(api, OnBufferMapReadAsyncCallback(apiBuffer, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallMapReadCallback(apiBuffer, DAWN_BUFFER_MAP_ASYNC_STATUS_ERROR, nullptr, 0);
        }));

    // Since the mapping failed, the handle is immediately destroyed.
    EXPECT_CALL(serverMemoryTransferService, OnReadHandleDestroy(serverHandle)).Times(1);

    FlushClient();

    // The client receives the map failure and destroys the handle.
    EXPECT_CALL(clientMemoryTransferService, OnReadHandleDestroy(clientHandle)).Times(1);

    FlushServer();

    dawnBufferUnmap(buffer);

    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();
}

// Test MapRead ReadHandle creation failure.
TEST_F(WireMemoryTransferServiceTests, BufferMapReadHandleCreationFailure) {
    DawnBuffer buffer;
    DawnBuffer apiBuffer;
    std::tie(apiBuffer, buffer) = CreateBuffer();
    FlushClient();

    // Mock a ReadHandle creation failure
    MockReadHandleCreationFailure();

    dawnBufferMapReadAsync(
        buffer,
        [](DawnBufferMapAsyncStatus status, const void* ptr, uint64_t dataLength, void* userdata) {
            EXPECT_EQ(status, DAWN_BUFFER_MAP_ASYNC_STATUS_CONTEXT_LOST);
            EXPECT_EQ(ptr, nullptr);
            EXPECT_EQ(dataLength, 0u);
        },
        nullptr);
}

// Test MapRead DeserializeReadHandle failure.
TEST_F(WireMemoryTransferServiceTests, BufferMapReadDeserializeReadHandleFailure) {
    DawnBuffer buffer;
    DawnBuffer apiBuffer;
    std::tie(apiBuffer, buffer) = CreateBuffer();
    FlushClient();

    // The client should create and serialize a ReadHandle on mapReadAsync.
    ClientReadHandle* clientHandle = ExpectReadHandleCreation();
    ExpectReadHandleSerialization(clientHandle);

    dawnBufferMapReadAsync(buffer,
                           [](DawnBufferMapAsyncStatus status, const void* ptr, uint64_t dataLength,
                              void* userdata) {},
                           nullptr);

    // Mock a Deserialization failure.
    MockServerReadHandleDeserializeFailure();

    FlushClient(false);

    EXPECT_CALL(clientMemoryTransferService, OnReadHandleDestroy(clientHandle)).Times(1);
}

// Test MapRead DeserializeInitialData failure.
TEST_F(WireMemoryTransferServiceTests, BufferMapReadDeserializeInitialDataFailure) {
    DawnBuffer buffer;
    DawnBuffer apiBuffer;
    std::tie(apiBuffer, buffer) = CreateBuffer();
    FlushClient();

    // The client should create and serialize a ReadHandle on mapReadAsync.
    ClientReadHandle* clientHandle = ExpectReadHandleCreation();
    ExpectReadHandleSerialization(clientHandle);

    dawnBufferMapReadAsync(buffer,
                           [](DawnBufferMapAsyncStatus status, const void* ptr, uint64_t dataLength,
                              void* userdata) {},
                           nullptr);

    // The server should deserialize the MapRead handle from the client and then serialize
    // an initialization message.
    ServerReadHandle* serverHandle = ExpectServerReadHandleDeserialize();
    ExpectServerReadHandleInitialize(serverHandle);

    // Mock a successful callback
    EXPECT_CALL(api, OnBufferMapReadAsyncCallback(apiBuffer, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallMapReadCallback(apiBuffer, DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS, &bufferContent,
                                    sizeof(bufferContent));
        }));

    FlushClient();

    // The client should receive the handle initialization message from the server.
    // Mock a deserialization failure.
    MockClientReadHandleDeserializeInitializeFailure(clientHandle);

    // The handle will be destroyed since deserializing failed.
    EXPECT_CALL(clientMemoryTransferService, OnReadHandleDestroy(clientHandle)).Times(1);

    FlushServer(false);

    EXPECT_CALL(serverMemoryTransferService, OnReadHandleDestroy(serverHandle)).Times(1);
}

// Test successful MapWrite.
TEST_F(WireMemoryTransferServiceTests, BufferMapWriteSuccess) {
    DawnBuffer buffer;
    DawnBuffer apiBuffer;
    std::tie(apiBuffer, buffer) = CreateBuffer();
    FlushClient();

    ClientWriteHandle* clientHandle = ExpectWriteHandleCreation();
    ExpectWriteHandleSerialization(clientHandle);

    dawnBufferMapWriteAsync(
        buffer,
        [](DawnBufferMapAsyncStatus status, void* ptr, uint64_t dataLength, void* userdata) {},
        nullptr);

    // The server should then deserialize the WriteHandle from the client.
    ServerWriteHandle* serverHandle = ExpectServerWriteHandleDeserialization();

    // Mock a successful callback.
    EXPECT_CALL(api, OnBufferMapWriteAsyncCallback(apiBuffer, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallMapWriteCallback(apiBuffer, DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS,
                                     &mappedBufferContent, sizeof(mappedBufferContent));
        }));

    FlushClient();

    // Since the mapping succeeds, the client opens the WriteHandle.
    ExpectClientWriteHandleOpen(clientHandle, &mappedBufferContent);

    FlushServer();

    // The client writes to the handle contents.
    mappedBufferContent = updatedBufferContent;

    // The client will then flush and destroy the handle on Unmap()
    ExpectClientWriteHandleSerializeFlush(clientHandle);
    EXPECT_CALL(clientMemoryTransferService, OnWriteHandleDestroy(clientHandle)).Times(1);

    dawnBufferUnmap(buffer);

    // The server deserializes the Flush message.
    ExpectServerWriteHandleDeserializeFlush(serverHandle, updatedBufferContent);

    // After the handle is updated it can be destroyed.
    EXPECT_CALL(serverMemoryTransferService, OnWriteHandleDestroy(serverHandle)).Times(1);
    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();
}

// Test unsuccessful MapWrite.
TEST_F(WireMemoryTransferServiceTests, BufferMapWriteError) {
    DawnBuffer buffer;
    DawnBuffer apiBuffer;
    std::tie(apiBuffer, buffer) = CreateBuffer();
    FlushClient();

    // The client should create and serialize a WriteHandle on mapWriteAsync.
    ClientWriteHandle* clientHandle = ExpectWriteHandleCreation();
    ExpectWriteHandleSerialization(clientHandle);

    dawnBufferMapWriteAsync(
        buffer,
        [](DawnBufferMapAsyncStatus status, void* ptr, uint64_t dataLength, void* userdata) {},
        nullptr);

    // The server should then deserialize the WriteHandle from the client.
    ServerWriteHandle* serverHandle = ExpectServerWriteHandleDeserialization();

    // Mock an error callback.
    EXPECT_CALL(api, OnBufferMapWriteAsyncCallback(apiBuffer, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallMapWriteCallback(apiBuffer, DAWN_BUFFER_MAP_ASYNC_STATUS_ERROR, nullptr, 0);
        }));

    // Since the mapping fails, the handle is immediately destroyed because it won't be written.
    EXPECT_CALL(serverMemoryTransferService, OnWriteHandleDestroy(serverHandle)).Times(1);

    FlushClient();

    // Client receives the map failure and destroys the handle.
    EXPECT_CALL(clientMemoryTransferService, OnWriteHandleDestroy(clientHandle)).Times(1);

    FlushServer();

    dawnBufferUnmap(buffer);

    EXPECT_CALL(api, BufferUnmap(apiBuffer)).Times(1);

    FlushClient();
}

// Test MapRead WriteHandle creation failure.
TEST_F(WireMemoryTransferServiceTests, BufferMapWriteHandleCreationFailure) {
    DawnBuffer buffer;
    DawnBuffer apiBuffer;
    std::tie(apiBuffer, buffer) = CreateBuffer();
    FlushClient();

    // Mock a WriteHandle creation failure
    MockWriteHandleCreationFailure();

    dawnBufferMapWriteAsync(
        buffer,
        [](DawnBufferMapAsyncStatus status, void* ptr, uint64_t dataLength, void* userdata) {
            EXPECT_EQ(status, DAWN_BUFFER_MAP_ASYNC_STATUS_CONTEXT_LOST);
            EXPECT_EQ(ptr, nullptr);
            EXPECT_EQ(dataLength, 0u);
        },
        nullptr);
}

// Test MapWrite DeserializeWriteHandle failure.
TEST_F(WireMemoryTransferServiceTests, BufferMapWriteDeserializeWriteHandleFailure) {
    DawnBuffer buffer;
    DawnBuffer apiBuffer;
    std::tie(apiBuffer, buffer) = CreateBuffer();
    FlushClient();

    // The client should create and serialize a WriteHandle on mapWriteAsync.
    ClientWriteHandle* clientHandle = ExpectWriteHandleCreation();
    ExpectWriteHandleSerialization(clientHandle);

    dawnBufferMapWriteAsync(
        buffer,
        [](DawnBufferMapAsyncStatus status, void* ptr, uint64_t dataLength, void* userdata) {},
        nullptr);

    // Mock a deserialization failure.
    MockServerWriteHandleDeserializeFailure();

    FlushClient(false);

    EXPECT_CALL(clientMemoryTransferService, OnWriteHandleDestroy(clientHandle)).Times(1);
}

// Test MapWrite handle Open failure.
TEST_F(WireMemoryTransferServiceTests, BufferMapWriteHandleOpenFailure) {
    DawnBuffer buffer;
    DawnBuffer apiBuffer;
    std::tie(apiBuffer, buffer) = CreateBuffer();
    FlushClient();

    ClientWriteHandle* clientHandle = ExpectWriteHandleCreation();
    ExpectWriteHandleSerialization(clientHandle);

    dawnBufferMapWriteAsync(
        buffer,
        [](DawnBufferMapAsyncStatus status, void* ptr, uint64_t dataLength, void* userdata) {},
        nullptr);

    // The server should then deserialize the WriteHandle from the client.
    ServerWriteHandle* serverHandle = ExpectServerWriteHandleDeserialization();

    // Mock a successful callback.
    EXPECT_CALL(api, OnBufferMapWriteAsyncCallback(apiBuffer, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallMapWriteCallback(apiBuffer, DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS,
                                     &mappedBufferContent, sizeof(mappedBufferContent));
        }));

    FlushClient();

    // Since the mapping succeeds, the client opens the WriteHandle.
    // Mock a failure.
    MockClientWriteHandleOpenFailure(clientHandle);

    // Since opening the handle fails, it gets destroyed immediately.
    EXPECT_CALL(clientMemoryTransferService, OnWriteHandleDestroy(clientHandle)).Times(1);

    FlushServer(false);

    EXPECT_CALL(serverMemoryTransferService, OnWriteHandleDestroy(serverHandle)).Times(1);
}

// Test MapWrite DeserializeFlush failure.
TEST_F(WireMemoryTransferServiceTests, BufferMapWriteDeserializeFlushFailure) {
    DawnBuffer buffer;
    DawnBuffer apiBuffer;
    std::tie(apiBuffer, buffer) = CreateBuffer();
    FlushClient();

    ClientWriteHandle* clientHandle = ExpectWriteHandleCreation();
    ExpectWriteHandleSerialization(clientHandle);

    dawnBufferMapWriteAsync(
        buffer,
        [](DawnBufferMapAsyncStatus status, void* ptr, uint64_t dataLength, void* userdata) {},
        nullptr);

    // The server should then deserialize the WriteHandle from the client.
    ServerWriteHandle* serverHandle = ExpectServerWriteHandleDeserialization();

    // Mock a successful callback.
    EXPECT_CALL(api, OnBufferMapWriteAsyncCallback(apiBuffer, _, _))
        .WillOnce(InvokeWithoutArgs([&]() {
            api.CallMapWriteCallback(apiBuffer, DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS,
                                     &mappedBufferContent, sizeof(mappedBufferContent));
        }));

    FlushClient();

    // Since the mapping succeeds, the client opens the WriteHandle.
    ExpectClientWriteHandleOpen(clientHandle, &mappedBufferContent);

    FlushServer();

    // The client writes to the handle contents.
    mappedBufferContent = updatedBufferContent;

    // The client will then flush and destroy the handle on Unmap()
    ExpectClientWriteHandleSerializeFlush(clientHandle);
    EXPECT_CALL(clientMemoryTransferService, OnWriteHandleDestroy(clientHandle)).Times(1);

    dawnBufferUnmap(buffer);

    // The server deserializes the Flush message. Mock a deserialization failure.
    MockServerWriteHandleDeserializeFlushFailure(serverHandle);

    FlushClient(false);

    EXPECT_CALL(serverMemoryTransferService, OnWriteHandleDestroy(serverHandle)).Times(1);
}

// Test successful CreateBufferMappedAsync.
TEST_F(WireMemoryTransferServiceTests, CreateBufferMappedAsyncSuccess) {
    // The client should create and serialize a WriteHandle on createBufferMappedAsync
    ClientWriteHandle* clientHandle = ExpectWriteHandleCreation();
    ExpectWriteHandleSerialization(clientHandle);

    DawnCreateBufferMappedResult result;
    DawnCreateBufferMappedResult apiResult = CreateBufferMappedAsync(&result);

    // The server should then deserialize the WriteHandle from the client.
    ServerWriteHandle* serverHandle = ExpectServerWriteHandleDeserialization();

    FlushClient();

    // Since the mapping succeeds, the client opens the WriteHandle.
    ExpectClientWriteHandleOpen(clientHandle, &mappedBufferContent);

    FlushServer();

    // The client writes to the handle contents.
    mappedBufferContent = updatedBufferContent;

    // The client will then flush and destroy the handle on Unmap()
    ExpectClientWriteHandleSerializeFlush(clientHandle);
    EXPECT_CALL(clientMemoryTransferService, OnWriteHandleDestroy(clientHandle)).Times(1);

    dawnBufferUnmap(result.buffer);

    // The server deserializes the Flush message.
    ExpectServerWriteHandleDeserializeFlush(serverHandle, updatedBufferContent);

    // After the handle is updated it can be destroyed.
    EXPECT_CALL(serverMemoryTransferService, OnWriteHandleDestroy(serverHandle)).Times(1);
    EXPECT_CALL(api, BufferUnmap(apiResult.buffer)).Times(1);

    FlushClient();
}

// Test CreateBufferMappedAsync WriteHandle creation failure.
TEST_F(WireMemoryTransferServiceTests, CreateBufferMappedAsyncWriteHandleCreationFailure) {
    // Mock a WriteHandle creation failure
    MockWriteHandleCreationFailure();

    DawnBufferDescriptor descriptor;
    descriptor.nextInChain = nullptr;
    descriptor.size = sizeof(bufferContent);

    dawnDeviceCreateBufferMappedAsync(
        device, &descriptor,
        [](DawnBufferMapAsyncStatus status, DawnCreateBufferMappedResult result, void* userdata) {
            EXPECT_EQ(status, DAWN_BUFFER_MAP_ASYNC_STATUS_CONTEXT_LOST);
            EXPECT_EQ(result.data, nullptr);
            EXPECT_EQ(result.dataLength, 0u);
        },
        nullptr);
}

// Test CreateBufferMappedAsync DeserializeWriteHandle failure.
TEST_F(WireMemoryTransferServiceTests, CreateBufferMappedAsyncDeserializeWriteHandleFailure) {
    // The client should create and serialize a WriteHandle on createBufferMappedAsync
    ClientWriteHandle* clientHandle = ExpectWriteHandleCreation();
    ExpectWriteHandleSerialization(clientHandle);

    DawnCreateBufferMappedResult result;
    DawnCreateBufferMappedResult apiResult = CreateBufferMappedAsync(&result);
    DAWN_UNUSED(apiResult);

    // The server should then deserialize the WriteHandle from the client.
    // Mock a deserialization failure.
    MockServerWriteHandleDeserializeFailure();

    FlushClient(false);

    EXPECT_CALL(clientMemoryTransferService, OnWriteHandleDestroy(clientHandle)).Times(1);
}

// Test CreateBufferMappedAsync handle Open failure.
TEST_F(WireMemoryTransferServiceTests, CreateBufferMappedAsyncHandleOpenFailure) {
    // The client should create and serialize a WriteHandle on createBufferMappedAsync
    ClientWriteHandle* clientHandle = ExpectWriteHandleCreation();
    ExpectWriteHandleSerialization(clientHandle);

    DawnCreateBufferMappedResult result;
    DawnCreateBufferMappedResult apiResult = CreateBufferMappedAsync(&result);
    DAWN_UNUSED(apiResult);

    // The server should then deserialize the WriteHandle from the client.
    ServerWriteHandle* serverHandle = ExpectServerWriteHandleDeserialization();

    FlushClient();

    // Since the mapping succeeds, the client opens the WriteHandle.
    MockClientWriteHandleOpenFailure(clientHandle);

    // Since opening the handle fails, it is destroyed immediately.
    EXPECT_CALL(clientMemoryTransferService, OnWriteHandleDestroy(clientHandle)).Times(1);

    FlushServer(false);

    EXPECT_CALL(serverMemoryTransferService, OnWriteHandleDestroy(serverHandle)).Times(1);
}

// Test CreateBufferMappedAsync DeserializeFlush failure.
TEST_F(WireMemoryTransferServiceTests, CreateBufferMappedAsyncDeserializeFlushFailure) {
    // The client should create and serialize a WriteHandle on createBufferMappedAsync
    ClientWriteHandle* clientHandle = ExpectWriteHandleCreation();
    ExpectWriteHandleSerialization(clientHandle);

    DawnCreateBufferMappedResult result;
    DawnCreateBufferMappedResult apiResult = CreateBufferMappedAsync(&result);
    DAWN_UNUSED(apiResult);

    // The server should then deserialize the WriteHandle from the client.
    ServerWriteHandle* serverHandle = ExpectServerWriteHandleDeserialization();

    FlushClient();

    // Since the mapping succeeds, the client opens the WriteHandle.
    ExpectClientWriteHandleOpen(clientHandle, &mappedBufferContent);

    FlushServer();

    // The client writes to the handle contents.
    mappedBufferContent = updatedBufferContent;

    // The client will then flush and destroy the handle on Unmap()
    ExpectClientWriteHandleSerializeFlush(clientHandle);
    EXPECT_CALL(clientMemoryTransferService, OnWriteHandleDestroy(clientHandle)).Times(1);

    dawnBufferUnmap(result.buffer);

    // The server deserializes the Flush message.
    // Mock a deserialization failure.
    MockServerWriteHandleDeserializeFlushFailure(serverHandle);

    FlushClient(false);

    EXPECT_CALL(serverMemoryTransferService, OnWriteHandleDestroy(serverHandle)).Times(1);
}

// Test successful CreateBufferMapped.
TEST_F(WireMemoryTransferServiceTests, CreateBufferMappedSuccess) {
    // The client should create and serialize a WriteHandle on createBufferMapped.
    ClientWriteHandle* clientHandle = ExpectWriteHandleCreation();

    // Staging data is immediately available so the handle is Opened.
    ExpectClientWriteHandleOpen(clientHandle, &mappedBufferContent);

    ExpectWriteHandleSerialization(clientHandle);

    // The server should then deserialize the WriteHandle from the client.
    ServerWriteHandle* serverHandle = ExpectServerWriteHandleDeserialization();

    DawnCreateBufferMappedResult result;
    DawnCreateBufferMappedResult apiResult;
    std::tie(apiResult, result) = CreateBufferMapped();
    FlushClient();

    // Update the mapped contents.
    mappedBufferContent = updatedBufferContent;

    // When the client Unmaps the buffer, it will flush writes to the handle and destroy it.
    ExpectClientWriteHandleSerializeFlush(clientHandle);
    EXPECT_CALL(clientMemoryTransferService, OnWriteHandleDestroy(clientHandle)).Times(1);

    dawnBufferUnmap(result.buffer);

    // The server deserializes the Flush message.
    ExpectServerWriteHandleDeserializeFlush(serverHandle, updatedBufferContent);

    // After the handle is updated it can be destroyed.
    EXPECT_CALL(serverMemoryTransferService, OnWriteHandleDestroy(serverHandle)).Times(1);
    EXPECT_CALL(api, BufferUnmap(apiResult.buffer)).Times(1);

    FlushClient();
}

// Test CreateBufferMapped WriteHandle creation failure.
TEST_F(WireMemoryTransferServiceTests, CreateBufferMappedWriteHandleCreationFailure) {
    // Mock a WriteHandle creation failure
    MockWriteHandleCreationFailure();

    DawnBufferDescriptor descriptor;
    descriptor.nextInChain = nullptr;
    descriptor.size = sizeof(bufferContent);

    DawnCreateBufferMappedResult result = dawnDeviceCreateBufferMapped(device, &descriptor);

    // TODO(enga): Check that the client generated a context lost.
    EXPECT_EQ(result.data, nullptr);
    EXPECT_EQ(result.dataLength, 0u);
}

// Test CreateBufferMapped DeserializeWriteHandle failure.
TEST_F(WireMemoryTransferServiceTests, CreateBufferMappedDeserializeWriteHandleFailure) {
    // The client should create and serialize a WriteHandle on createBufferMapped.
    ClientWriteHandle* clientHandle = ExpectWriteHandleCreation();

    // Staging data is immediately available so the handle is Opened.
    ExpectClientWriteHandleOpen(clientHandle, &mappedBufferContent);

    ExpectWriteHandleSerialization(clientHandle);

    // The server should then deserialize the WriteHandle from the client.
    MockServerWriteHandleDeserializeFailure();

    DawnCreateBufferMappedResult result;
    DawnCreateBufferMappedResult apiResult;
    std::tie(apiResult, result) = CreateBufferMapped();
    FlushClient(false);

    EXPECT_CALL(clientMemoryTransferService, OnWriteHandleDestroy(clientHandle)).Times(1);
}

// Test CreateBufferMapped handle Open failure.
TEST_F(WireMemoryTransferServiceTests, CreateBufferMappedHandleOpenFailure) {
    // The client should create a WriteHandle on createBufferMapped.
    ClientWriteHandle* clientHandle = ExpectWriteHandleCreation();

    // Staging data is immediately available so the handle is Opened.
    // Mock a failure.
    MockClientWriteHandleOpenFailure(clientHandle);

    // Since synchronous opening of the handle failed, it is destroyed immediately.
    // Note: The handle is not serialized because sychronously opening it failed.
    EXPECT_CALL(clientMemoryTransferService, OnWriteHandleDestroy(clientHandle)).Times(1);

    DawnBufferDescriptor descriptor;
    descriptor.nextInChain = nullptr;
    descriptor.size = sizeof(bufferContent);

    DawnCreateBufferMappedResult result = dawnDeviceCreateBufferMapped(device, &descriptor);

    // TODO(enga): Check that the client generated a context lost.
    EXPECT_EQ(result.data, nullptr);
    EXPECT_EQ(result.dataLength, 0u);
}

// Test CreateBufferMapped DeserializeFlush failure.
TEST_F(WireMemoryTransferServiceTests, CreateBufferMappedDeserializeFlushFailure) {
    // The client should create and serialize a WriteHandle on createBufferMapped.
    ClientWriteHandle* clientHandle = ExpectWriteHandleCreation();

    // Staging data is immediately available so the handle is Opened.
    ExpectClientWriteHandleOpen(clientHandle, &mappedBufferContent);

    ExpectWriteHandleSerialization(clientHandle);

    // The server should then deserialize the WriteHandle from the client.
    ServerWriteHandle* serverHandle = ExpectServerWriteHandleDeserialization();

    DawnCreateBufferMappedResult result;
    DawnCreateBufferMappedResult apiResult;
    std::tie(apiResult, result) = CreateBufferMapped();
    FlushClient();

    // Update the mapped contents.
    mappedBufferContent = updatedBufferContent;

    // When the client Unmaps the buffer, it will flush writes to the handle and destroy it.
    ExpectClientWriteHandleSerializeFlush(clientHandle);
    EXPECT_CALL(clientMemoryTransferService, OnWriteHandleDestroy(clientHandle)).Times(1);

    dawnBufferUnmap(result.buffer);

    // The server deserializes the Flush message. Mock a deserialization failure.
    MockServerWriteHandleDeserializeFlushFailure(serverHandle);

    FlushClient(false);

    EXPECT_CALL(serverMemoryTransferService, OnWriteHandleDestroy(serverHandle)).Times(1);
}
