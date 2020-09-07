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

#include "utils/SystemUtils.h"
#include "utils/WGPUHelpers.h"

WGPUDevice device;
WGPUQueue queue;
WGPUSwapChain swapchain;
WGPURenderPipeline pipeline;

WGPUTextureFormat swapChainFormat;

void init() {
    device = CreateCppDawnDevice().Release();
    queue = wgpuDeviceGetDefaultQueue(device);

    {
        WGPUSwapChainDescriptor descriptor = {};
        descriptor.implementation = GetSwapChainImplementation();
        swapchain = wgpuDeviceCreateSwapChain(device, nullptr, &descriptor);
    }
    swapChainFormat = static_cast<WGPUTextureFormat>(GetPreferredSwapChainTextureFormat());
    wgpuSwapChainConfigure(swapchain, swapChainFormat, WGPUTextureUsage_OutputAttachment, 640, 480);

    const char* wgsl =
        "var<private> pos : array<vec2<f32>, 3> = array<vec2<f32>, 3>(\n"
        "        vec2<f32>(0.0, 0.5),\n"
        "        vec2<f32>(-0.5, -0.5),\n"
        "        vec2<f32>(0.5, -0.5));\n"
        "[[builtin position]] var<out> Position : vec4<f32>;\n"
        "[[builtin vertex_idx]] var<in> VertexIndex : i32;\n"
        "fn vtx_main() -> void {\n"
        "    Position = vec4<f32>(pos[VertexIndex], 0.0, 1.0);\n"
        "    return;\n"
        "}\n"
        "entry_point vertex = vtx_main; \n"
        "\n"
        "[[location 0]] var<out> outColor : vec4<f32>;\n"
        "fn frag_main() -> void {\n"
        "  outColor = vec4<f32>(1.0, 0.0, 0.0, 1.0);\n"
        "  return;\n"
        "}\n"
        "entry_point fragment = frag_main;\n";

    WGPUShaderModule module =
        utils::CreateShaderModuleFromWGSL(wgpu::Device(device), wgsl).Release();

    {
        WGPURenderPipelineDescriptor descriptor = {};

        descriptor.vertexStage.module = module;
        descriptor.vertexStage.entryPoint = "vtx_main";

        WGPUProgrammableStageDescriptor fragmentStage = {};
        fragmentStage.module = module;
        fragmentStage.entryPoint = "frag_main";
        descriptor.fragmentStage = &fragmentStage;

        descriptor.sampleCount = 1;

        WGPUBlendDescriptor blendDescriptor = {};
        blendDescriptor.operation = WGPUBlendOperation_Add;
        blendDescriptor.srcFactor = WGPUBlendFactor_One;
        blendDescriptor.dstFactor = WGPUBlendFactor_One;
        WGPUColorStateDescriptor colorStateDescriptor = {};
        colorStateDescriptor.format = swapChainFormat;
        colorStateDescriptor.alphaBlend = blendDescriptor;
        colorStateDescriptor.colorBlend = blendDescriptor;
        colorStateDescriptor.writeMask = WGPUColorWriteMask_All;

        descriptor.colorStateCount = 1;
        descriptor.colorStates = &colorStateDescriptor;

        WGPUPipelineLayoutDescriptor pl = {};
        pl.bindGroupLayoutCount = 0;
        pl.bindGroupLayouts = nullptr;
        descriptor.layout = wgpuDeviceCreatePipelineLayout(device, &pl);

        WGPUVertexStateDescriptor vertexState = {};
        vertexState.indexFormat = WGPUIndexFormat_Uint32;
        vertexState.vertexBufferCount = 0;
        vertexState.vertexBuffers = nullptr;
        descriptor.vertexState = &vertexState;

        WGPURasterizationStateDescriptor rasterizationState = {};
        rasterizationState.frontFace = WGPUFrontFace_CCW;
        rasterizationState.cullMode = WGPUCullMode_None;
        rasterizationState.depthBias = 0;
        rasterizationState.depthBiasSlopeScale = 0.0;
        rasterizationState.depthBiasClamp = 0.0;
        descriptor.rasterizationState = &rasterizationState;

        descriptor.primitiveTopology = WGPUPrimitiveTopology_TriangleList;
        descriptor.sampleMask = 0xFFFFFFFF;
        descriptor.alphaToCoverageEnabled = false;

        descriptor.depthStencilState = nullptr;

        pipeline = wgpuDeviceCreateRenderPipeline(device, &descriptor);
    }

    wgpuShaderModuleRelease(module);
}

void frame() {
    WGPUTextureView backbufferView = wgpuSwapChainGetCurrentTextureView(swapchain);
    WGPURenderPassDescriptor renderpassInfo = {};
    WGPURenderPassColorAttachmentDescriptor colorAttachment = {};
    {
        colorAttachment.attachment = backbufferView;
        colorAttachment.resolveTarget = nullptr;
        colorAttachment.clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        renderpassInfo.colorAttachmentCount = 1;
        renderpassInfo.colorAttachments = &colorAttachment;
        renderpassInfo.depthStencilAttachment = nullptr;
    }
    WGPUCommandBuffer commands;
    {
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);

        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &renderpassInfo);
        wgpuRenderPassEncoderSetPipeline(pass, pipeline);
        wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
        wgpuRenderPassEncoderEndPass(pass);
        wgpuRenderPassEncoderRelease(pass);

        commands = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuCommandEncoderRelease(encoder);
    }

    wgpuQueueSubmit(queue, 1, &commands);
    wgpuCommandBufferRelease(commands);
    wgpuSwapChainPresent(swapchain);
    wgpuTextureViewRelease(backbufferView);

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
