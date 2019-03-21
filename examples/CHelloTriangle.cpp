// Copyright 2017 The Dawn Authors
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

#include "SampleUtils.h"

#include "utils/DawnHelpers.h"
#include "utils/SystemUtils.h"

DawnDevice device;
DawnQueue queue;
DawnSwapChain swapchain;
DawnRenderPipeline pipeline;

DawnTextureFormat swapChainFormat;

DawnTextureView depthStencilView;

void init() {
    device = CreateCppDawnDevice().Release();
    queue = dawnDeviceCreateQueue(device);

    {
        DawnSwapChainDescriptor descriptor;
        descriptor.nextInChain = nullptr;
        descriptor.implementation = GetSwapChainImplementation();
        swapchain = dawnDeviceCreateSwapChain(device, &descriptor);
    }
    swapChainFormat = static_cast<DawnTextureFormat>(GetPreferredSwapChainTextureFormat());
    dawnSwapChainConfigure(swapchain, swapChainFormat, DAWN_TEXTURE_USAGE_BIT_OUTPUT_ATTACHMENT, 640,
                          480);

    const char* vs =
        R"(#version 450
        const vec3 pos[6] = vec3[6](vec3(-1.f, -1.f, 1.0f),
                                     vec3(-1.f, 1.f, 0.5f),
                                     vec3(1.f, -1.f, 0.5f),
                                     vec3(1.f, -1.f, 0.5f),
                                     vec3(-1.f, 1.f, 0.5f),
                                     vec3(1.f, 1.f, 0.f));
        void main() {
           gl_Position = vec4(pos[gl_VertexIndex], 1.0);
       })";
    DawnShaderModule vsModule = utils::CreateShaderModule(dawn::Device(device), dawn::ShaderStage::Vertex, vs).Release();

    const char* fs =
        "#version 450\n"
        "layout(location = 0) out vec4 fragColor;"
        "void main() {\n"
        "   fragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";
    DawnShaderModule fsModule = utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, fs).Release();

    {
        DawnRenderPipelineDescriptor descriptor;
        descriptor.nextInChain = nullptr;

        DawnPipelineStageDescriptor vertexStage;
        vertexStage.nextInChain = nullptr;
        vertexStage.module = vsModule;
        vertexStage.entryPoint = "main";
        descriptor.vertexStage = &vertexStage;

        DawnPipelineStageDescriptor fragmentStage;
        fragmentStage.nextInChain = nullptr;
        fragmentStage.module = fsModule;
        fragmentStage.entryPoint = "main";
        descriptor.fragmentStage = &fragmentStage;

        descriptor.sampleCount = 1;

        DawnBlendDescriptor blendDescriptor;
        blendDescriptor.operation = DAWN_BLEND_OPERATION_ADD;
        blendDescriptor.srcFactor = DAWN_BLEND_FACTOR_ONE;
        blendDescriptor.dstFactor = DAWN_BLEND_FACTOR_ONE;
        DawnColorStateDescriptor colorStateDescriptor;
        colorStateDescriptor.nextInChain = nullptr;
        colorStateDescriptor.format = swapChainFormat;
        colorStateDescriptor.alphaBlend = blendDescriptor;
        colorStateDescriptor.colorBlend = blendDescriptor;
        colorStateDescriptor.colorWriteMask = DAWN_COLOR_WRITE_MASK_ALL;

        descriptor.colorStateCount = 1;
        DawnColorStateDescriptor* colorStatesPtr[] = {&colorStateDescriptor};
        descriptor.colorStates = colorStatesPtr;

        DawnDepthStencilStateDescriptor depthStencilStateDescriptor;
        depthStencilStateDescriptor.format = DAWN_TEXTURE_FORMAT_D32_FLOAT_S8_UINT;
        DawnStencilStateFaceDescriptor stencilFace;
        stencilFace.compare = DAWN_COMPARE_FUNCTION_ALWAYS;
        stencilFace.failOp = DAWN_STENCIL_OPERATION_KEEP;
        stencilFace.depthFailOp = DAWN_STENCIL_OPERATION_KEEP;
        stencilFace.passOp = DAWN_STENCIL_OPERATION_KEEP;
        depthStencilStateDescriptor.depthWriteEnabled = true;
        depthStencilStateDescriptor.depthCompare = DAWN_COMPARE_FUNCTION_LESS_EQUAL;
        depthStencilStateDescriptor.stencilBack = stencilFace;
        depthStencilStateDescriptor.stencilFront = stencilFace;
        depthStencilStateDescriptor.stencilReadMask = 0xff;
        depthStencilStateDescriptor.stencilWriteMask = 0xff;
        depthStencilStateDescriptor.nextInChain = nullptr;
        descriptor.depthStencilState = &depthStencilStateDescriptor;

        DawnPipelineLayoutDescriptor pl;
        pl.nextInChain = nullptr;
        pl.bindGroupLayoutCount = 0;
        pl.bindGroupLayouts = nullptr;
        descriptor.layout = dawnDeviceCreatePipelineLayout(device, &pl);

        DawnInputStateBuilder inputStateBuilder = dawnDeviceCreateInputStateBuilder(device);
        descriptor.inputState = dawnInputStateBuilderGetResult(inputStateBuilder);
        dawnInputStateBuilderRelease(inputStateBuilder);

        descriptor.indexFormat = DAWN_INDEX_FORMAT_UINT32;
        descriptor.primitiveTopology = DAWN_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        //descriptor.depthStencilState = nullptr;

        pipeline = dawnDeviceCreateRenderPipeline(device, &descriptor);

        dawnInputStateRelease(descriptor.inputState);
    }

    dawnShaderModuleRelease(vsModule);
    dawnShaderModuleRelease(fsModule);

    {
        DawnTextureDescriptor descriptor;
        descriptor.nextInChain = nullptr;
        descriptor.arrayLayerCount = 1;
        descriptor.mipLevelCount = 1;
        descriptor.dimension = DAWN_TEXTURE_DIMENSION_2D;
        descriptor.format = DAWN_TEXTURE_FORMAT_D32_FLOAT_S8_UINT;
        descriptor.sampleCount = 1;
        descriptor.size = { 640, 480, 1 };
        descriptor.usage = DAWN_TEXTURE_USAGE_BIT_OUTPUT_ATTACHMENT;
        auto depthStencilTexture = dawnDeviceCreateTexture(device, &descriptor);
        depthStencilView = dawnTextureCreateDefaultTextureView(depthStencilTexture);
    }

}

void frame() {
    DawnTexture backbuffer = dawnSwapChainGetNextTexture(swapchain);
    DawnTextureView backbufferView;
    {
        backbufferView = dawnTextureCreateDefaultTextureView(backbuffer);
    }
    DawnRenderPassDescriptor renderpassInfo;
    DawnRenderPassColorAttachmentDescriptor colorAttachment;
    DawnRenderPassColorAttachmentDescriptor* colorAttachments = {&colorAttachment};
    DawnRenderPassDepthStencilAttachmentDescriptor depthStencilAttachment;
    depthStencilAttachment.attachment = depthStencilView;
    depthStencilAttachment.clearDepth = 0.75f;
    depthStencilAttachment.clearStencil = 0;
    depthStencilAttachment.depthLoadOp = DAWN_LOAD_OP_CLEAR;
    depthStencilAttachment.depthStoreOp = DAWN_STORE_OP_STORE;
    depthStencilAttachment.stencilLoadOp = DAWN_LOAD_OP_LOAD;
    depthStencilAttachment.stencilStoreOp = DAWN_STORE_OP_STORE;

    {
        colorAttachment.attachment = backbufferView;
        colorAttachment.resolveTarget = nullptr;
        colorAttachment.clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        colorAttachment.loadOp = DAWN_LOAD_OP_CLEAR;
        colorAttachment.storeOp = DAWN_STORE_OP_STORE;
        renderpassInfo.colorAttachmentCount = 1;
        renderpassInfo.colorAttachments = &colorAttachments;
        renderpassInfo.depthStencilAttachment = &depthStencilAttachment;
    }
    DawnCommandBuffer commands;
    {
        DawnCommandEncoder encoder = dawnDeviceCreateCommandEncoder(device);

        DawnRenderPassEncoder pass = dawnCommandEncoderBeginRenderPass(encoder, &renderpassInfo);
        dawnRenderPassEncoderSetPipeline(pass, pipeline);
        dawnRenderPassEncoderDraw(pass, 6, 1, 0, 0);
        dawnRenderPassEncoderEndPass(pass);
        dawnRenderPassEncoderRelease(pass);

        commands = dawnCommandEncoderFinish(encoder);
        dawnCommandEncoderRelease(encoder);
    }

    dawnQueueSubmit(queue, 1, &commands);
    dawnCommandBufferRelease(commands);
    dawnSwapChainPresent(swapchain, backbuffer);
    dawnTextureViewRelease(backbufferView);

    DoFlush();
}

int main(int argc, const char* argv[]) {
    if (!InitSample(argc, argv)) {
        return 1;
    }
    init();

    while (!ShouldQuit()) {
        frame();
        utils::USleep(16000);
    }

    // TODO release stuff
}
