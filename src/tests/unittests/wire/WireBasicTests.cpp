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

#include "common/Constants.h"

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

// Test that the wire is able to send numerical values
TEST_F(WireBasicTests, ValueArgument) {
    dawnCommandBufferBuilder builder = dawnDeviceCreateCommandBufferBuilder(device);
    dawnComputePassEncoder pass = dawnCommandBufferBuilderBeginComputePass(builder);
    dawnComputePassEncoderDispatch(pass, 1, 2, 3);

    dawnCommandBufferBuilder apiBuilder = api.GetNewCommandBufferBuilder();
    EXPECT_CALL(api, DeviceCreateCommandBufferBuilder(apiDevice)).WillOnce(Return(apiBuilder));

    dawnComputePassEncoder apiPass = api.GetNewComputePassEncoder();
    EXPECT_CALL(api, CommandBufferBuilderBeginComputePass(apiBuilder)).WillOnce(Return(apiPass));

    EXPECT_CALL(api, ComputePassEncoderDispatch(apiPass, 1, 2, 3)).Times(1);

    EXPECT_CALL(api, CommandBufferBuilderRelease(apiBuilder));
    EXPECT_CALL(api, ComputePassEncoderRelease(apiPass));
    FlushClient();
}

// Test that the wire is able to send arrays of numerical values
static constexpr uint32_t testPushConstantValues[4] = {0, 42, 0xDEADBEEFu, 0xFFFFFFFFu};

bool CheckPushConstantValues(const uint32_t* values) {
    for (int i = 0; i < 4; ++i) {
        if (values[i] != testPushConstantValues[i]) {
            return false;
        }
    }
    return true;
}

TEST_F(WireBasicTests, ValueArrayArgument) {
    dawnCommandBufferBuilder builder = dawnDeviceCreateCommandBufferBuilder(device);
    dawnComputePassEncoder pass = dawnCommandBufferBuilderBeginComputePass(builder);
    dawnComputePassEncoderSetPushConstants(pass, DAWN_SHADER_STAGE_BIT_VERTEX, 0, 4,
                                           testPushConstantValues);

    dawnCommandBufferBuilder apiBuilder = api.GetNewCommandBufferBuilder();
    EXPECT_CALL(api, DeviceCreateCommandBufferBuilder(apiDevice)).WillOnce(Return(apiBuilder));

    dawnComputePassEncoder apiPass = api.GetNewComputePassEncoder();
    EXPECT_CALL(api, CommandBufferBuilderBeginComputePass(apiBuilder)).WillOnce(Return(apiPass));

    EXPECT_CALL(api,
                ComputePassEncoderSetPushConstants(apiPass, DAWN_SHADER_STAGE_BIT_VERTEX, 0, 4,
                                                   ResultOf(CheckPushConstantValues, Eq(true))));
    EXPECT_CALL(api, CommandBufferBuilderRelease(apiBuilder));
    EXPECT_CALL(api, ComputePassEncoderRelease(apiPass));

    FlushClient();
}

// Test that the wire is able to send C strings
TEST_F(WireBasicTests, CStringArgument) {
    // Create shader module
    dawnShaderModuleDescriptor vertexDescriptor;
    vertexDescriptor.nextInChain = nullptr;
    vertexDescriptor.codeSize = 0;
    dawnShaderModule vsModule = dawnDeviceCreateShaderModule(device, &vertexDescriptor);
    dawnShaderModule apiVsModule = api.GetNewShaderModule();
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiVsModule));

    // Create the blend state descriptor
    dawnBlendDescriptor blendDescriptor;
    blendDescriptor.operation = DAWN_BLEND_OPERATION_ADD;
    blendDescriptor.srcFactor = DAWN_BLEND_FACTOR_ONE;
    blendDescriptor.dstFactor = DAWN_BLEND_FACTOR_ONE;
    dawnBlendStateDescriptor blendStateDescriptor;
    blendStateDescriptor.nextInChain = nullptr;
    blendStateDescriptor.alphaBlend = blendDescriptor;
    blendStateDescriptor.colorBlend = blendDescriptor;
    blendStateDescriptor.colorWriteMask = DAWN_COLOR_WRITE_MASK_ALL;

    // Create the input state
    dawnInputStateBuilder inputStateBuilder = dawnDeviceCreateInputStateBuilder(device);
    dawnInputStateBuilder apiInputStateBuilder = api.GetNewInputStateBuilder();
    EXPECT_CALL(api, DeviceCreateInputStateBuilder(apiDevice))
        .WillOnce(Return(apiInputStateBuilder));

    dawnInputState inputState = dawnInputStateBuilderGetResult(inputStateBuilder);
    dawnInputState apiInputState = api.GetNewInputState();
    EXPECT_CALL(api, InputStateBuilderGetResult(apiInputStateBuilder))
        .WillOnce(Return(apiInputState));

    // Create the depth-stencil state
    dawnStencilStateFaceDescriptor stencilFace;
    stencilFace.compare = DAWN_COMPARE_FUNCTION_ALWAYS;
    stencilFace.failOp = DAWN_STENCIL_OPERATION_KEEP;
    stencilFace.depthFailOp = DAWN_STENCIL_OPERATION_KEEP;
    stencilFace.passOp = DAWN_STENCIL_OPERATION_KEEP;

    dawnDepthStencilStateDescriptor depthStencilState;
    depthStencilState.nextInChain = nullptr;
    depthStencilState.depthWriteEnabled = false;
    depthStencilState.depthCompare = DAWN_COMPARE_FUNCTION_ALWAYS;
    depthStencilState.stencilBack = stencilFace;
    depthStencilState.stencilFront = stencilFace;
    depthStencilState.stencilReadMask = 0xff;
    depthStencilState.stencilWriteMask = 0xff;

    // Create the pipeline layout
    dawnPipelineLayoutDescriptor layoutDescriptor;
    layoutDescriptor.nextInChain = nullptr;
    layoutDescriptor.numBindGroupLayouts = 0;
    layoutDescriptor.bindGroupLayouts = nullptr;
    dawnPipelineLayout layout = dawnDeviceCreatePipelineLayout(device, &layoutDescriptor);
    dawnPipelineLayout apiLayout = api.GetNewPipelineLayout();
    EXPECT_CALL(api, DeviceCreatePipelineLayout(apiDevice, _)).WillOnce(Return(apiLayout));

    // Create pipeline
    dawnRenderPipelineDescriptor pipelineDescriptor;
    pipelineDescriptor.nextInChain = nullptr;

    dawnPipelineStageDescriptor vertexStage;
    vertexStage.nextInChain = nullptr;
    vertexStage.module = vsModule;
    vertexStage.entryPoint = "main";
    pipelineDescriptor.vertexStage = &vertexStage;

    dawnPipelineStageDescriptor fragmentStage;
    fragmentStage.nextInChain = nullptr;
    fragmentStage.module = vsModule;
    fragmentStage.entryPoint = "main";
    pipelineDescriptor.fragmentStage = &fragmentStage;

    dawnAttachmentsStateDescriptor attachmentsState;
    attachmentsState.nextInChain = nullptr;
    attachmentsState.numColorAttachments = 1;
    dawnAttachmentDescriptor colorAttachment = {nullptr, DAWN_TEXTURE_FORMAT_R8_G8_B8_A8_UNORM};
    dawnAttachmentDescriptor* colorAttachmentPtr[] = {&colorAttachment};
    attachmentsState.colorAttachments = colorAttachmentPtr;
    attachmentsState.hasDepthStencilAttachment = false;
    // Even with hasDepthStencilAttachment = false, depthStencilAttachment must point to valid
    // data because we don't have optional substructures yet.
    attachmentsState.depthStencilAttachment = &colorAttachment;
    pipelineDescriptor.attachmentsState = &attachmentsState;

    pipelineDescriptor.numBlendStates = 1;
    pipelineDescriptor.blendStates = &blendStateDescriptor;

    pipelineDescriptor.sampleCount = 1;
    pipelineDescriptor.layout = layout;
    pipelineDescriptor.inputState = inputState;
    pipelineDescriptor.indexFormat = DAWN_INDEX_FORMAT_UINT32;
    pipelineDescriptor.primitiveTopology = DAWN_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineDescriptor.depthStencilState = &depthStencilState;

    dawnDeviceCreateRenderPipeline(device, &pipelineDescriptor);
    EXPECT_CALL(api,
                DeviceCreateRenderPipeline(
                    apiDevice, MatchesLambda([](const dawnRenderPipelineDescriptor* desc) -> bool {
                        return desc->vertexStage->entryPoint == std::string("main");
                    })))
        .WillOnce(Return(nullptr));
    EXPECT_CALL(api, ShaderModuleRelease(apiVsModule));
    EXPECT_CALL(api, InputStateBuilderRelease(apiInputStateBuilder));
    EXPECT_CALL(api, InputStateRelease(apiInputState));
    EXPECT_CALL(api, PipelineLayoutRelease(apiLayout));

    FlushClient();
}

// Test that the wire is able to send objects as value arguments
TEST_F(WireBasicTests, ObjectAsValueArgument) {
    // Create a RenderPassDescriptor
    dawnRenderPassDescriptorBuilder renderPassBuilder =
        dawnDeviceCreateRenderPassDescriptorBuilder(device);
    dawnRenderPassDescriptor renderPass =
        dawnRenderPassDescriptorBuilderGetResult(renderPassBuilder);

    dawnRenderPassDescriptorBuilder apiRenderPassBuilder = api.GetNewRenderPassDescriptorBuilder();
    EXPECT_CALL(api, DeviceCreateRenderPassDescriptorBuilder(apiDevice))
        .WillOnce(Return(apiRenderPassBuilder));
    dawnRenderPassDescriptor apiRenderPass = api.GetNewRenderPassDescriptor();
    EXPECT_CALL(api, RenderPassDescriptorBuilderGetResult(apiRenderPassBuilder))
        .WillOnce(Return(apiRenderPass));

    // Create command buffer builder, setting render pass descriptor
    dawnCommandBufferBuilder cmdBufBuilder = dawnDeviceCreateCommandBufferBuilder(device);
    dawnCommandBufferBuilderBeginRenderPass(cmdBufBuilder, renderPass);

    dawnCommandBufferBuilder apiCmdBufBuilder = api.GetNewCommandBufferBuilder();
    EXPECT_CALL(api, DeviceCreateCommandBufferBuilder(apiDevice))
        .WillOnce(Return(apiCmdBufBuilder));

    EXPECT_CALL(api, CommandBufferBuilderBeginRenderPass(apiCmdBufBuilder, apiRenderPass)).Times(1);

    EXPECT_CALL(api, CommandBufferBuilderRelease(apiCmdBufBuilder));
    EXPECT_CALL(api, RenderPassDescriptorBuilderRelease(apiRenderPassBuilder));
    EXPECT_CALL(api, RenderPassDescriptorRelease(apiRenderPass));
    FlushClient();
}

// Test that the wire is able to send array of objects
TEST_F(WireBasicTests, ObjectsAsPointerArgument) {
    dawnCommandBuffer cmdBufs[2];
    dawnCommandBuffer apiCmdBufs[2];

    // Create two command buffers we need to use a GMock sequence otherwise the order of the
    // CreateCommandBufferBuilder might be swapped since they are equivalent in term of matchers
    Sequence s;
    for (int i = 0; i < 2; ++i) {
        dawnCommandBufferBuilder cmdBufBuilder = dawnDeviceCreateCommandBufferBuilder(device);
        cmdBufs[i] = dawnCommandBufferBuilderGetResult(cmdBufBuilder);

        dawnCommandBufferBuilder apiCmdBufBuilder = api.GetNewCommandBufferBuilder();
        EXPECT_CALL(api, DeviceCreateCommandBufferBuilder(apiDevice))
            .InSequence(s)
            .WillOnce(Return(apiCmdBufBuilder));

        apiCmdBufs[i] = api.GetNewCommandBuffer();
        EXPECT_CALL(api, CommandBufferBuilderGetResult(apiCmdBufBuilder))
            .WillOnce(Return(apiCmdBufs[i]));
        EXPECT_CALL(api, CommandBufferBuilderRelease(apiCmdBufBuilder));
        EXPECT_CALL(api, CommandBufferRelease(apiCmdBufs[i]));
    }

    // Create queue
    dawnQueue queue = dawnDeviceCreateQueue(device);
    dawnQueue apiQueue = api.GetNewQueue();
    EXPECT_CALL(api, DeviceCreateQueue(apiDevice)).WillOnce(Return(apiQueue));

    // Submit command buffer and check we got a call with both API-side command buffers
    dawnQueueSubmit(queue, 2, cmdBufs);

    EXPECT_CALL(
        api, QueueSubmit(apiQueue, 2, MatchesLambda([=](const dawnCommandBuffer* cmdBufs) -> bool {
                             return cmdBufs[0] == apiCmdBufs[0] && cmdBufs[1] == apiCmdBufs[1];
                         })));

    EXPECT_CALL(api, QueueRelease(apiQueue));
    FlushClient();
}

// Test that the wire is able to send structures that contain pure values (non-objects)
TEST_F(WireBasicTests, StructureOfValuesArgument) {
    dawnSamplerDescriptor descriptor;
    descriptor.nextInChain = nullptr;
    descriptor.magFilter = DAWN_FILTER_MODE_LINEAR;
    descriptor.minFilter = DAWN_FILTER_MODE_NEAREST;
    descriptor.mipmapFilter = DAWN_FILTER_MODE_LINEAR;
    descriptor.addressModeU = DAWN_ADDRESS_MODE_CLAMP_TO_EDGE;
    descriptor.addressModeV = DAWN_ADDRESS_MODE_REPEAT;
    descriptor.addressModeW = DAWN_ADDRESS_MODE_MIRRORED_REPEAT;
    descriptor.lodMinClamp = kLodMin;
    descriptor.lodMaxClamp = kLodMax;
    descriptor.compareFunction = DAWN_COMPARE_FUNCTION_NEVER;
    descriptor.borderColor = DAWN_BORDER_COLOR_TRANSPARENT_BLACK;

    dawnDeviceCreateSampler(device, &descriptor);
    EXPECT_CALL(api, DeviceCreateSampler(
                         apiDevice, MatchesLambda([](const dawnSamplerDescriptor* desc) -> bool {
                             return desc->nextInChain == nullptr &&
                                    desc->magFilter == DAWN_FILTER_MODE_LINEAR &&
                                    desc->minFilter == DAWN_FILTER_MODE_NEAREST &&
                                    desc->mipmapFilter == DAWN_FILTER_MODE_LINEAR &&
                                    desc->addressModeU == DAWN_ADDRESS_MODE_CLAMP_TO_EDGE &&
                                    desc->addressModeV == DAWN_ADDRESS_MODE_REPEAT &&
                                    desc->addressModeW == DAWN_ADDRESS_MODE_MIRRORED_REPEAT &&
                                    desc->compareFunction == DAWN_COMPARE_FUNCTION_NEVER &&
                                    desc->borderColor == DAWN_BORDER_COLOR_TRANSPARENT_BLACK &&
                                    desc->lodMinClamp == kLodMin && desc->lodMaxClamp == kLodMax;
                         })))
        .WillOnce(Return(nullptr));

    FlushClient();
}

// Test that the wire is able to send structures that contain objects
TEST_F(WireBasicTests, StructureOfObjectArrayArgument) {
    dawnBindGroupLayoutDescriptor bglDescriptor;
    bglDescriptor.numBindings = 0;
    bglDescriptor.bindings = nullptr;

    dawnBindGroupLayout bgl = dawnDeviceCreateBindGroupLayout(device, &bglDescriptor);
    dawnBindGroupLayout apiBgl = api.GetNewBindGroupLayout();
    EXPECT_CALL(api, DeviceCreateBindGroupLayout(apiDevice, _)).WillOnce(Return(apiBgl));

    dawnPipelineLayoutDescriptor descriptor;
    descriptor.nextInChain = nullptr;
    descriptor.numBindGroupLayouts = 1;
    descriptor.bindGroupLayouts = &bgl;

    dawnDeviceCreatePipelineLayout(device, &descriptor);
    EXPECT_CALL(api, DeviceCreatePipelineLayout(
                         apiDevice,
                         MatchesLambda([apiBgl](const dawnPipelineLayoutDescriptor* desc) -> bool {
                             return desc->nextInChain == nullptr &&
                                    desc->numBindGroupLayouts == 1 &&
                                    desc->bindGroupLayouts[0] == apiBgl;
                         })))
        .WillOnce(Return(nullptr));

    EXPECT_CALL(api, BindGroupLayoutRelease(apiBgl));
    FlushClient();
}

// Test that the wire is able to send structures that contain objects
TEST_F(WireBasicTests, StructureOfStructureArrayArgument) {
    static constexpr int NUM_BINDINGS = 3;
    dawnBindGroupLayoutBinding bindings[NUM_BINDINGS]{
        {0, DAWN_SHADER_STAGE_BIT_VERTEX, DAWN_BINDING_TYPE_SAMPLER},
        {1, DAWN_SHADER_STAGE_BIT_VERTEX, DAWN_BINDING_TYPE_SAMPLED_TEXTURE},
        {2,
         static_cast<dawnShaderStageBit>(DAWN_SHADER_STAGE_BIT_VERTEX |
                                         DAWN_SHADER_STAGE_BIT_FRAGMENT),
         DAWN_BINDING_TYPE_UNIFORM_BUFFER},
    };
    dawnBindGroupLayoutDescriptor bglDescriptor;
    bglDescriptor.numBindings = NUM_BINDINGS;
    bglDescriptor.bindings = bindings;

    dawnDeviceCreateBindGroupLayout(device, &bglDescriptor);
    dawnBindGroupLayout apiBgl = api.GetNewBindGroupLayout();
    EXPECT_CALL(
        api,
        DeviceCreateBindGroupLayout(
            apiDevice, MatchesLambda([bindings](const dawnBindGroupLayoutDescriptor* desc) -> bool {
                for (int i = 0; i < NUM_BINDINGS; ++i) {
                    const auto& a = desc->bindings[i];
                    const auto& b = bindings[i];
                    if (a.binding != b.binding || a.visibility != b.visibility ||
                        a.type != b.type) {
                        return false;
                    }
                }
                return desc->nextInChain == nullptr && desc->numBindings == 3;
            })))
        .WillOnce(Return(apiBgl));

    EXPECT_CALL(api, BindGroupLayoutRelease(apiBgl));
    FlushClient();
}

// Test passing nullptr instead of objects - object as value version
TEST_F(WireBasicTests, OptionalObjectValue) {
    dawnBindGroupLayoutDescriptor bglDesc;
    bglDesc.nextInChain = nullptr;
    bglDesc.numBindings = 0;
    dawnBindGroupLayout bgl = dawnDeviceCreateBindGroupLayout(device, &bglDesc);

    dawnBindGroupLayout apiBindGroupLayout = api.GetNewBindGroupLayout();
    EXPECT_CALL(api, DeviceCreateBindGroupLayout(apiDevice, _))
        .WillOnce(Return(apiBindGroupLayout));

    // The `sampler`, `textureView` and `buffer` members of a binding are optional.
    dawnBindGroupBinding binding;
    binding.binding = 0;
    binding.sampler = nullptr;
    binding.textureView = nullptr;
    binding.buffer = nullptr;

    dawnBindGroupDescriptor bgDesc;
    bgDesc.nextInChain = nullptr;
    bgDesc.layout = bgl;
    bgDesc.numBindings = 1;
    bgDesc.bindings = &binding;

    dawnDeviceCreateBindGroup(device, &bgDesc);
    EXPECT_CALL(api, DeviceCreateBindGroup(
                         apiDevice, MatchesLambda([](const dawnBindGroupDescriptor* desc) -> bool {
                             return desc->nextInChain == nullptr && desc->numBindings == 1 &&
                                    desc->bindings[0].binding == 0 &&
                                    desc->bindings[0].sampler == nullptr &&
                                    desc->bindings[0].buffer == nullptr &&
                                    desc->bindings[0].textureView == nullptr;
                         })))
        .WillOnce(Return(nullptr));

    EXPECT_CALL(api, BindGroupLayoutRelease(apiBindGroupLayout));
    FlushClient();
}

// Test passing nullptr instead of objects - array of objects version
TEST_F(WireBasicTests, DISABLED_NullptrInArray) {
    dawnBindGroupLayout nullBGL = nullptr;

    dawnPipelineLayoutDescriptor descriptor;
    descriptor.nextInChain = nullptr;
    descriptor.numBindGroupLayouts = 1;
    descriptor.bindGroupLayouts = &nullBGL;

    dawnDeviceCreatePipelineLayout(device, &descriptor);
    EXPECT_CALL(api,
                DeviceCreatePipelineLayout(
                    apiDevice, MatchesLambda([](const dawnPipelineLayoutDescriptor* desc) -> bool {
                        return desc->nextInChain == nullptr && desc->numBindGroupLayouts == 1 &&
                               desc->bindGroupLayouts[0] == nullptr;
                    })))
        .WillOnce(Return(nullptr));

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

// Test that we get a success builder error status when no error happens
TEST_F(WireBasicTests, SuccessCallbackOnBuilderSuccess) {
    dawnBufferBuilder bufferBuilder = dawnDeviceCreateBufferBuilderForTesting(device);
    dawnBufferBuilderSetErrorCallback(bufferBuilder, ToMockBuilderErrorCallback, 1, 2);
    dawnBufferBuilderGetResult(bufferBuilder);

    dawnBufferBuilder apiBufferBuilder = api.GetNewBufferBuilder();
    EXPECT_CALL(api, DeviceCreateBufferBuilderForTesting(apiDevice))
        .WillOnce(Return(apiBufferBuilder));

    dawnBuffer apiBuffer = api.GetNewBuffer();
    EXPECT_CALL(api, BufferBuilderGetResult(apiBufferBuilder))
        .WillOnce(InvokeWithoutArgs([&]() -> dawnBuffer {
            api.CallBuilderErrorCallback(apiBufferBuilder, DAWN_BUILDER_ERROR_STATUS_SUCCESS,
                                         "I like cheese");
            return apiBuffer;
        }));

    EXPECT_CALL(api, BufferBuilderRelease(apiBufferBuilder));
    EXPECT_CALL(api, BufferRelease(apiBuffer));
    FlushClient();

    EXPECT_CALL(*mockBuilderErrorCallback, Call(DAWN_BUILDER_ERROR_STATUS_SUCCESS, _, 1, 2));

    FlushServer();
}

// Test that the client calls the builder callback with unknown when it HAS to fire the callback but
// can't know the status yet.
TEST_F(WireBasicTests, UnknownBuilderErrorStatusCallback) {
    // The builder is destroyed before the object is built
    {
        dawnBufferBuilder bufferBuilder = dawnDeviceCreateBufferBuilderForTesting(device);
        dawnBufferBuilderSetErrorCallback(bufferBuilder, ToMockBuilderErrorCallback, 1, 2);

        EXPECT_CALL(*mockBuilderErrorCallback, Call(DAWN_BUILDER_ERROR_STATUS_UNKNOWN, _, 1, 2))
            .Times(1);

        dawnBufferBuilderRelease(bufferBuilder);
    }

    // If the builder has been consumed, it doesn't fire the callback with unknown
    {
        dawnBufferBuilder bufferBuilder = dawnDeviceCreateBufferBuilderForTesting(device);
        dawnBufferBuilderSetErrorCallback(bufferBuilder, ToMockBuilderErrorCallback, 3, 4);
        dawnBufferBuilderGetResult(bufferBuilder);

        EXPECT_CALL(*mockBuilderErrorCallback, Call(DAWN_BUILDER_ERROR_STATUS_UNKNOWN, _, 3, 4))
            .Times(0);

        dawnBufferBuilderRelease(bufferBuilder);
    }

    // If the builder has been consumed, and the object is destroyed before the result comes from
    // the server, then the callback is fired with unknown
    {
        dawnBufferBuilder bufferBuilder = dawnDeviceCreateBufferBuilderForTesting(device);
        dawnBufferBuilderSetErrorCallback(bufferBuilder, ToMockBuilderErrorCallback, 5, 6);
        dawnBuffer buffer = dawnBufferBuilderGetResult(bufferBuilder);

        EXPECT_CALL(*mockBuilderErrorCallback, Call(DAWN_BUILDER_ERROR_STATUS_UNKNOWN, _, 5, 6))
            .Times(1);

        dawnBufferRelease(buffer);
    }
}

// Test that a builder success status doesn't get forwarded to the device
TEST_F(WireBasicTests, SuccessCallbackNotForwardedToDevice) {
    dawnDeviceSetErrorCallback(device, ToMockDeviceErrorCallback, 0);

    dawnBufferBuilder bufferBuilder = dawnDeviceCreateBufferBuilderForTesting(device);
    dawnBufferBuilderGetResult(bufferBuilder);

    dawnBufferBuilder apiBufferBuilder = api.GetNewBufferBuilder();
    EXPECT_CALL(api, DeviceCreateBufferBuilderForTesting(apiDevice))
        .WillOnce(Return(apiBufferBuilder));

    dawnBuffer apiBuffer = api.GetNewBuffer();
    EXPECT_CALL(api, BufferBuilderGetResult(apiBufferBuilder))
        .WillOnce(InvokeWithoutArgs([&]() -> dawnBuffer {
            api.CallBuilderErrorCallback(apiBufferBuilder, DAWN_BUILDER_ERROR_STATUS_SUCCESS,
                                         "I like cheese");
            return apiBuffer;
        }));

    EXPECT_CALL(api, BufferBuilderRelease(apiBufferBuilder));
    EXPECT_CALL(api, BufferRelease(apiBuffer));
    FlushClient();
    FlushServer();
}

// Test that a builder error status gets forwarded to the device
TEST_F(WireBasicTests, ErrorCallbackForwardedToDevice) {
    uint64_t userdata = 30495;
    dawnDeviceSetErrorCallback(device, ToMockDeviceErrorCallback, userdata);

    dawnBufferBuilder bufferBuilder = dawnDeviceCreateBufferBuilderForTesting(device);
    dawnBufferBuilderGetResult(bufferBuilder);

    dawnBufferBuilder apiBufferBuilder = api.GetNewBufferBuilder();
    EXPECT_CALL(api, DeviceCreateBufferBuilderForTesting(apiDevice))
        .WillOnce(Return(apiBufferBuilder));

    EXPECT_CALL(api, BufferBuilderGetResult(apiBufferBuilder))
        .WillOnce(InvokeWithoutArgs([&]() -> dawnBuffer {
            api.CallBuilderErrorCallback(apiBufferBuilder, DAWN_BUILDER_ERROR_STATUS_ERROR,
                                         "Error :(");
            return nullptr;
        }));

    EXPECT_CALL(api, BufferBuilderRelease(apiBufferBuilder));
    FlushClient();

    EXPECT_CALL(*mockDeviceErrorCallback, Call(_, userdata)).Times(1);

    FlushServer();
}
