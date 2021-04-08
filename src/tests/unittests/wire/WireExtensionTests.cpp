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

#include "utils/ComboRenderPipelineDescriptor.h"

using namespace testing;
using namespace dawn_wire;

class WireExtensionTests : public WireTest {
  public:
    WireExtensionTests() {
    }
    ~WireExtensionTests() override = default;
};

// Serialize/Deserializes a chained struct correctly.
TEST_F(WireExtensionTests, ChainedStruct) {
    WGPUShaderModuleDescriptor shaderModuleDesc = {};
    WGPUShaderModule apiShaderModule = api.GetNewShaderModule();
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiShaderModule));
    FlushClient();

    WGPUPrimitiveDepthClampingState clientExt = {};
    clientExt.chain.sType = WGPUSType_PrimitiveDepthClampingState;
    clientExt.chain.next = nullptr;
    clientExt.clampDepth = true;

    WGPURenderPipelineDescriptor2 renderPipelineDesc = {};
    renderPipelineDesc.vertex.module = shaderModule;
    renderPipelineDesc.vertex.entryPoint = "main";
    renderPipelineDesc.primitive.nextInChain = &clientExt.chain;

    wgpuDeviceCreateRenderPipeline2(device, &renderPipelineDesc);
    EXPECT_CALL(api, DeviceCreateRenderPipeline2(apiDevice, NotNull()))
        .WillOnce(Invoke([&](Unused,
                             const WGPURenderPipelineDescriptor2* serverDesc) -> WGPURenderPipeline {
            const auto* ext = reinterpret_cast<const WGPUPrimitiveDepthClampingState*>(
                serverDesc->primitive.nextInChain);
            EXPECT_EQ(ext->chain.sType, clientExt.chain.sType);
            EXPECT_EQ(ext->clampDepth, true);
            EXPECT_EQ(ext->chain.next, nullptr);

            return api.GetNewRenderPipeline();
        }));
    FlushClient();
}

// Serialize/Deserializes multiple chained structs correctly.
TEST_F(WireExtensionTests, MutlipleChainedStructs) {
    WGPUShaderModuleDescriptor shaderModuleDesc = {};
    WGPUShaderModule apiShaderModule = api.GetNewShaderModule();
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiShaderModule));
    FlushClient();

    WGPUPrimitiveDepthClampingState clientExt2 = {};
    clientExt2.chain.sType = WGPUSType_PrimitiveDepthClampingState;
    clientExt2.chain.next = nullptr;
    clientExt2.clampDepth = false;

    WGPUPrimitiveDepthClampingState clientExt1 = {};
    clientExt1.chain.sType = WGPUSType_PrimitiveDepthClampingState;
    clientExt1.chain.next = &clientExt2.chain;
    clientExt1.clampDepth = true;

    WGPURenderPipelineDescriptor2 renderPipelineDesc = {};
    renderPipelineDesc.vertex.module = shaderModule;
    renderPipelineDesc.vertex.entryPoint = "main";
    renderPipelineDesc.primitive.nextInChain = &clientExt1.chain;

    wgpuDeviceCreateRenderPipeline2(device, &renderPipelineDesc);
    EXPECT_CALL(api, DeviceCreateRenderPipeline2(apiDevice, NotNull()))
        .WillOnce(Invoke([&](Unused,
                             const WGPURenderPipelineDescriptor2* serverDesc) -> WGPURenderPipeline {
            const auto* ext1 = reinterpret_cast<const WGPUPrimitiveDepthClampingState*>(
                serverDesc->primitive.nextInChain);
            EXPECT_EQ(ext1->chain.sType, clientExt1.chain.sType);
            EXPECT_EQ(ext1->clampDepth, true);

            const auto* ext2 = reinterpret_cast<const WGPUPrimitiveDepthClampingState*>(
                ext1->chain.next);
            EXPECT_EQ(ext2->chain.sType, clientExt2.chain.sType);
            EXPECT_EQ(ext2->clampDepth, false);
            EXPECT_EQ(ext2->chain.next, nullptr);

            return api.GetNewRenderPipeline();
        }));
    FlushClient();

    // Swap the order of the chained structs.
    renderPipelineDesc.primitive.nextInChain = &clientExt2.chain;
    clientExt2.chain.next = &clientExt1.chain;
    clientExt1.chain.next = nullptr;

    wgpuDeviceCreateRenderPipeline2(device, &renderPipelineDesc);
    EXPECT_CALL(api, DeviceCreateRenderPipeline2(apiDevice, NotNull()))
        .WillOnce(Invoke([&](Unused,
                             const WGPURenderPipelineDescriptor2* serverDesc) -> WGPURenderPipeline {
            const auto* ext2 = reinterpret_cast<const WGPUPrimitiveDepthClampingState*>(
                serverDesc->primitive.nextInChain);
            EXPECT_EQ(ext2->chain.sType, clientExt2.chain.sType);
            EXPECT_EQ(ext2->clampDepth, false);

            const auto* ext1 = reinterpret_cast<const WGPUPrimitiveDepthClampingState*>(
                ext2->chain.next);
            EXPECT_EQ(ext1->chain.sType, clientExt1.chain.sType);
            EXPECT_EQ(ext1->clampDepth, true);
            EXPECT_EQ(ext1->chain.next, nullptr);

            return api.GetNewRenderPipeline();
        }));
    FlushClient();
}

// Test that a chained struct with Invalid sType passes through as Invalid.
TEST_F(WireExtensionTests, InvalidSType) {
    WGPUShaderModuleDescriptor shaderModuleDesc = {};
    WGPUShaderModule apiShaderModule = api.GetNewShaderModule();
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiShaderModule));
    FlushClient();

    WGPUPrimitiveDepthClampingState clientExt = {};
    clientExt.chain.sType = WGPUSType_Invalid;
    clientExt.chain.next = nullptr;

    WGPURenderPipelineDescriptor2 renderPipelineDesc = {};
    renderPipelineDesc.vertex.module = shaderModule;
    renderPipelineDesc.vertex.entryPoint = "main";
    renderPipelineDesc.primitive.nextInChain = &clientExt.chain;

    wgpuDeviceCreateRenderPipeline2(device, &renderPipelineDesc);
    EXPECT_CALL(api, DeviceCreateRenderPipeline2(apiDevice, NotNull()))
        .WillOnce(Invoke([&](Unused,
                             const WGPURenderPipelineDescriptor2* serverDesc) -> WGPURenderPipeline {
            EXPECT_EQ(serverDesc->primitive.nextInChain->sType, WGPUSType_Invalid);
            EXPECT_EQ(serverDesc->primitive.nextInChain->next, nullptr);
            return api.GetNewRenderPipeline();
        }));
    FlushClient();
}

// Test that a chained struct with unknown sType passes through as Invalid.
TEST_F(WireExtensionTests, UnknownSType) {
    WGPUShaderModuleDescriptor shaderModuleDesc = {};
    WGPUShaderModule apiShaderModule = api.GetNewShaderModule();
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiShaderModule));
    FlushClient();

    WGPUPrimitiveDepthClampingState clientExt = {};
    clientExt.chain.sType = static_cast<WGPUSType>(-1);
    clientExt.chain.next = nullptr;

    WGPURenderPipelineDescriptor2 renderPipelineDesc = {};
    renderPipelineDesc.vertex.module = shaderModule;
    renderPipelineDesc.vertex.entryPoint = "main";
    renderPipelineDesc.primitive.nextInChain = &clientExt.chain;

    wgpuDeviceCreateRenderPipeline2(device, &renderPipelineDesc);
    EXPECT_CALL(api, DeviceCreateRenderPipeline2(apiDevice, NotNull()))
        .WillOnce(Invoke([&](Unused,
                             const WGPURenderPipelineDescriptor2* serverDesc) -> WGPURenderPipeline {
            EXPECT_EQ(serverDesc->primitive.nextInChain->sType, WGPUSType_Invalid);
            EXPECT_EQ(serverDesc->primitive.nextInChain->next, nullptr);
            return api.GetNewRenderPipeline();
        }));
    FlushClient();
}

// Test that if both an invalid and valid stype are passed on the chain, it is an error.
TEST_F(WireExtensionTests, ValidAndInvalidSTypeInChain) {
    WGPUShaderModuleDescriptor shaderModuleDesc = {};
    WGPUShaderModule apiShaderModule = api.GetNewShaderModule();
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiShaderModule));
    FlushClient();

    WGPUPrimitiveDepthClampingState clientExt2 = {};
    clientExt2.chain.sType = WGPUSType_Invalid;
    clientExt2.chain.next = nullptr;

    WGPUPrimitiveDepthClampingState clientExt1 = {};
    clientExt1.chain.sType = WGPUSType_PrimitiveDepthClampingState;
    clientExt1.chain.next = &clientExt2.chain;
    clientExt1.clampDepth = true;

    WGPURenderPipelineDescriptor2 renderPipelineDesc = {};
    renderPipelineDesc.vertex.module = shaderModule;
    renderPipelineDesc.vertex.entryPoint = "main";
    renderPipelineDesc.primitive.nextInChain = &clientExt1.chain;

    wgpuDeviceCreateRenderPipeline2(device, &renderPipelineDesc);
    EXPECT_CALL(api, DeviceCreateRenderPipeline2(apiDevice, NotNull()))
        .WillOnce(Invoke([&](Unused,
                             const WGPURenderPipelineDescriptor2* serverDesc) -> WGPURenderPipeline {
            const auto* ext = reinterpret_cast<const WGPUPrimitiveDepthClampingState*>(
                serverDesc->primitive.nextInChain);
            EXPECT_EQ(ext->chain.sType, clientExt1.chain.sType);
            EXPECT_EQ(ext->clampDepth, true);

            EXPECT_EQ(ext->chain.next->sType, WGPUSType_Invalid);
            EXPECT_EQ(ext->chain.next->next, nullptr);
            return api.GetNewRenderPipeline();
        }));
    FlushClient();

    // Swap the order of the chained structs.
    renderPipelineDesc.primitive.nextInChain = &clientExt2.chain;
    clientExt2.chain.next = &clientExt1.chain;
    clientExt1.chain.next = nullptr;

    wgpuDeviceCreateRenderPipeline2(device, &renderPipelineDesc);
    EXPECT_CALL(api, DeviceCreateRenderPipeline2(apiDevice, NotNull()))
        .WillOnce(Invoke([&](Unused,
                             const WGPURenderPipelineDescriptor2* serverDesc) -> WGPURenderPipeline {
            EXPECT_EQ(serverDesc->primitive.nextInChain->sType, WGPUSType_Invalid);

            const auto* ext = reinterpret_cast<const WGPUPrimitiveDepthClampingState*>(
                serverDesc->primitive.nextInChain->next);
            EXPECT_EQ(ext->chain.sType, clientExt1.chain.sType);
            EXPECT_EQ(ext->clampDepth, true);
            EXPECT_EQ(ext->chain.next, nullptr);

            return api.GetNewRenderPipeline();
        }));
    FlushClient();
}

// Test that (de)?serializing a chained struct with subdescriptors works.
TEST_F(WireExtensionTests, ChainedStructWithSubdescriptor) {
    WGPUShaderModuleDescriptor shaderModuleDesc = {};

    WGPUShaderModule apiShaderModule1 = api.GetNewShaderModule();
    WGPUShaderModule shaderModule1 = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiShaderModule1));
    FlushClient();

    WGPUShaderModule apiShaderModule2 = api.GetNewShaderModule();
    WGPUShaderModule shaderModule2 = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    EXPECT_CALL(api, DeviceCreateShaderModule(apiDevice, _)).WillOnce(Return(apiShaderModule2));
    FlushClient();

    WGPUProgrammableStageDescriptor extraStageDesc = {};
    extraStageDesc.module = shaderModule1;
    extraStageDesc.entryPoint = "my other module";

    WGPURenderPipelineDescriptorDummyExtension clientExt = {};
    clientExt.chain.sType = WGPUSType_RenderPipelineDescriptorDummyExtension;
    clientExt.chain.next = nullptr;
    clientExt.dummyStage = extraStageDesc;

    WGPURenderPipelineDescriptor renderPipelineDesc = {};
    renderPipelineDesc.nextInChain = &clientExt.chain;
    renderPipelineDesc.vertexStage.module = shaderModule2;
    renderPipelineDesc.vertexStage.entryPoint = "my vertex module";

    wgpuDeviceCreateRenderPipeline(device, &renderPipelineDesc);
    EXPECT_CALL(api, DeviceCreateRenderPipeline(apiDevice, NotNull()))
        .WillOnce(Invoke([&](Unused,
                             const WGPURenderPipelineDescriptor* serverDesc) -> WGPURenderPipeline {
            EXPECT_EQ(serverDesc->vertexStage.module, apiShaderModule2);
            EXPECT_STREQ(serverDesc->vertexStage.entryPoint,
                         renderPipelineDesc.vertexStage.entryPoint);

            const auto* ext = reinterpret_cast<const WGPURenderPipelineDescriptorDummyExtension*>(
                serverDesc->nextInChain);
            EXPECT_EQ(ext->chain.sType, clientExt.chain.sType);
            EXPECT_EQ(ext->dummyStage.module, apiShaderModule1);
            EXPECT_STREQ(ext->dummyStage.entryPoint, extraStageDesc.entryPoint);

            EXPECT_EQ(ext->chain.next, nullptr);

            return api.GetNewRenderPipeline();
        }));
    FlushClient();
}
