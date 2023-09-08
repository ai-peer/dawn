// Copyright 2023 The Dawn Authors
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
#include "RendererC.h"
#include "dawn/common/Log.h"  // NOLINT

void RendererC::Init() {
    device = CreateCppDawnDeviceForAndroid(androidDesc).MoveToCHandle();
    queue = wgpuDeviceGetQueue(device);
    swapChain = GetSwapChain().MoveToCHandle();

    const char* shaderSource = R"(
    @vertex
    fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4<f32> {
      var p = vec2f(0.0, 0.0);
      if (in_vertex_index == 0u) {
          p = vec2f(-0.5, -0.5);
      } else if (in_vertex_index == 1u) {
          p = vec2f(0.5, -0.5);
      } else {
          p = vec2f(0.0, 0.5);
      }
      return vec4f(p, 0.0, 1.0);
    }

    @fragment
    fn fs_main() -> @location(0) vec4f {
      return vec4f(0.0, 0.4, 1.0, 1.0);
    }
    )";

    WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {};
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    shaderCodeDesc.code = shaderSource;
    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = &shaderCodeDesc.chain;

    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.nextInChain = nullptr;

    // Vertex fetch
    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;

    // Vertex shader
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;

    // Fragment shader
    WGPUFragmentState fragmentState = {};
    fragmentState.nextInChain = nullptr;
    pipelineDesc.fragment = &fragmentState;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    // Configure blend state
    WGPUBlendState blendState;
    blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.color.operation = WGPUBlendOperation_Add;
    blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
    blendState.alpha.dstFactor = WGPUBlendFactor_One;
    blendState.alpha.operation = WGPUBlendOperation_Add;

    WGPUColorTargetState colorTarget = {};
    colorTarget.nextInChain = nullptr;
    colorTarget.format = static_cast<WGPUTextureFormat>(GetPreferredSwapChainTextureFormat());
    colorTarget.blend = &blendState;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    pipelineDesc.depthStencil = nullptr;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    pipelineDesc.layout = nullptr;
    pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
}

void RendererC::Frame() {
    WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(swapChain);
    if (!nextTexture) {
        dawn::ErrorLog() << "Cannot acquire next swap chain texture";
        return;
    }

    WGPURenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = nextTexture;
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
    renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
    renderPassColorAttachment.clearValue = WGPUColor{0.9, 0.1, 0.2, 1.0};

    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.nextInChain = nullptr;
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;

    WGPUCommandEncoderDescriptor commandEncoderDesc = {};
    commandEncoderDesc.nextInChain = nullptr;
    commandEncoderDesc.label = "Command Encoder";
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &commandEncoderDesc);
    WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
    wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);
    wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(renderPass);

    wgpuTextureViewRelease(nextTexture);

    WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
    cmdBufferDescriptor.nextInChain = nullptr;
    cmdBufferDescriptor.label = "Command buffer";

    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
    wgpuQueueSubmit(queue, 1, &command);
    wgpuSwapChainPresent(swapChain);
}

void RendererC::GameLoop() {
    if (!deviceInitialised) {
        Init();
        deviceInitialised = true;
    }
    Frame();
}
