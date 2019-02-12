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

using namespace testing;
using namespace dawn_wire;

class WireBasicTests : public WireTest {
  public:
    WireBasicTests() : WireTest(true) {
    }
    ~WireBasicTests() override = default;
};

// One call gets forwarded correctly.
TEST_F(WireBasicTests, CallForwarded) {
    dawnDeviceCreateCommandBufferBuilder(device);

    dawnCommandBufferBuilder apiCmdBufBuilder = api.GetNewCommandBufferBuilder();
    EXPECT_CALL(api, DeviceCreateCommandBufferBuilder(apiDevice))
        .WillOnce(Return(apiCmdBufBuilder));

    EXPECT_CALL(api, CommandBufferBuilderRelease(apiCmdBufBuilder));
    FlushClient();
}

// Test that calling methods on a new object works as expected.
TEST_F(WireBasicTests, CreateThenCall) {
    dawnCommandBufferBuilder builder = dawnDeviceCreateCommandBufferBuilder(device);
    dawnCommandBufferBuilderGetResult(builder);

    dawnCommandBufferBuilder apiCmdBufBuilder = api.GetNewCommandBufferBuilder();
    EXPECT_CALL(api, DeviceCreateCommandBufferBuilder(apiDevice))
        .WillOnce(Return(apiCmdBufBuilder));

    dawnCommandBuffer apiCmdBuf = api.GetNewCommandBuffer();
    EXPECT_CALL(api, CommandBufferBuilderGetResult(apiCmdBufBuilder)).WillOnce(Return(apiCmdBuf));

    EXPECT_CALL(api, CommandBufferBuilderRelease(apiCmdBufBuilder));
    EXPECT_CALL(api, CommandBufferRelease(apiCmdBuf));
    FlushClient();
}

// Test that client reference/release do not call the backend API.
TEST_F(WireBasicTests, RefCountKeptInClient) {
    dawnCommandBufferBuilder builder = dawnDeviceCreateCommandBufferBuilder(device);

    dawnCommandBufferBuilderReference(builder);
    dawnCommandBufferBuilderRelease(builder);

    dawnCommandBufferBuilder apiCmdBufBuilder = api.GetNewCommandBufferBuilder();
    EXPECT_CALL(api, DeviceCreateCommandBufferBuilder(apiDevice))
        .WillOnce(Return(apiCmdBufBuilder));
    EXPECT_CALL(api, CommandBufferBuilderRelease(apiCmdBufBuilder));

    FlushClient();
}

// Test that client reference/release do not call the backend API.
TEST_F(WireBasicTests, ReleaseCalledOnRefCount0) {
    dawnCommandBufferBuilder builder = dawnDeviceCreateCommandBufferBuilder(device);

    dawnCommandBufferBuilderRelease(builder);

    dawnCommandBufferBuilder apiCmdBufBuilder = api.GetNewCommandBufferBuilder();
    EXPECT_CALL(api, DeviceCreateCommandBufferBuilder(apiDevice))
        .WillOnce(Return(apiCmdBufBuilder));

    EXPECT_CALL(api, CommandBufferBuilderRelease(apiCmdBufBuilder));

    FlushClient();
}

// Test that the server doesn't forward calls to error objects or with error objects
// Also test that when GetResult is called on an error builder, the error callback is fired
// TODO(cwallez@chromium.org): This test is disabled because the introduction of encoders breaks
// the assumptions of the "builder error" handling that a builder is self-contained. We need to
// revisit this once the new error handling is in place.
TEST_F(WireBasicTests, DISABLED_CallsSkippedAfterBuilderError) {
    dawnCommandBufferBuilder cmdBufBuilder = dawnDeviceCreateCommandBufferBuilder(device);
    dawnCommandBufferBuilderSetErrorCallback(cmdBufBuilder, ToMockBuilderErrorCallback, 1, 2);

    dawnRenderPassEncoder pass = dawnCommandBufferBuilderBeginRenderPass(cmdBufBuilder, nullptr);

    dawnBufferBuilder bufferBuilder = dawnDeviceCreateBufferBuilderForTesting(device);
    dawnBufferBuilderSetErrorCallback(bufferBuilder, ToMockBuilderErrorCallback, 3, 4);
    dawnBuffer buffer = dawnBufferBuilderGetResult(bufferBuilder);  // Hey look an error!

    // These calls will be skipped because of the error
    dawnBufferSetSubData(buffer, 0, 0, nullptr);
    dawnRenderPassEncoderSetIndexBuffer(pass, buffer, 0);
    dawnRenderPassEncoderEndPass(pass);
    dawnCommandBufferBuilderGetResult(cmdBufBuilder);

    dawnCommandBufferBuilder apiCmdBufBuilder = api.GetNewCommandBufferBuilder();
    EXPECT_CALL(api, DeviceCreateCommandBufferBuilder(apiDevice))
        .WillOnce(Return(apiCmdBufBuilder));

    dawnRenderPassEncoder apiPass = api.GetNewRenderPassEncoder();
    EXPECT_CALL(api, CommandBufferBuilderBeginRenderPass(apiCmdBufBuilder, _))
        .WillOnce(Return(apiPass));

    dawnBufferBuilder apiBufferBuilder = api.GetNewBufferBuilder();
    EXPECT_CALL(api, DeviceCreateBufferBuilderForTesting(apiDevice))
        .WillOnce(Return(apiBufferBuilder));

    // Hey look an error!
    EXPECT_CALL(api, BufferBuilderGetResult(apiBufferBuilder))
        .WillOnce(InvokeWithoutArgs([&]() -> dawnBuffer {
            api.CallBuilderErrorCallback(apiBufferBuilder, DAWN_BUILDER_ERROR_STATUS_ERROR,
                                         "Error");
            return nullptr;
        }));

    EXPECT_CALL(api, BufferSetSubData(_, _, _, _)).Times(0);
    EXPECT_CALL(api, RenderPassEncoderSetIndexBuffer(_, _, _)).Times(0);
    EXPECT_CALL(api, CommandBufferBuilderGetResult(_)).Times(0);

    FlushClient();

    EXPECT_CALL(*mockBuilderErrorCallback, Call(DAWN_BUILDER_ERROR_STATUS_ERROR, _, 1, 2)).Times(1);
    EXPECT_CALL(*mockBuilderErrorCallback, Call(DAWN_BUILDER_ERROR_STATUS_ERROR, _, 3, 4)).Times(1);

    FlushServer();
}
