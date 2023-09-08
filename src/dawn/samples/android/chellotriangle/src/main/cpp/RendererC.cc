#include "RendererC.h"
#include <dawn/webgpu.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include "UtilitiesC.cc"

void RendererC::initRenderer() {
    mApp->textInputState = 0;
    androidDesc.window = mApp->window;
    androidDesc.chain.sType = WGPUSType_SurfaceDescriptorFromAndroidNativeWindow;

    WGPUSurfaceDescriptor surfaceDesc = {};
    surfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&androidDesc);
    WGPUSurface surface = wgpuInstanceCreateSurface(instance, &surfaceDesc);

    adapterOpts.backendType = WGPUBackendType_Vulkan;
    adapterOpts.compatibleSurface = surface;
    adapter = requestAdapter(instance, &adapterOpts);
    deviceDesc.label = "Test Device";
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredLimits = nullptr;
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "The default queue";
    device = requestDevice(adapter, &deviceDesc);
    queue = wgpuDeviceGetQueue(device);

    auto onDeviceError = [](WGPUErrorType type, char const* message, void* /* pUserData */) {
        LOGI("Uncaptured device error:...");
        if (message) {
            LOGI("%s\n", message);
        }
    };
    wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr /* pUserData */);

    LOGI("Creating swapchain device...");

    WGPUSwapChainDescriptor swapChainDesc = {};
    swapChainDesc.nextInChain = nullptr;
    swapChainDesc.width = 640;
    swapChainDesc.height = 480;
    swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;

    // The swap chain textures use the color format suggested by the target
    // surface. Currently, dawn accepts only RGBA8Unorm for Android.
    WGPUTextureFormat swapChainFormat = WGPUTextureFormat_RGBA8Unorm;
    swapChainDesc.format = swapChainFormat;
    swapChainDesc.presentMode = WGPUPresentMode_Fifo;
    swapChain = wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);

    LOGI("Creating shader module...");
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
    colorTarget.format = swapChainFormat;
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

void RendererC::GameLoop() {
    if (!deviceInitialised) {
        initRenderer();
        deviceInitialised = true;
    }

    WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(swapChain);
    if (!nextTexture) {
        LOGI("Cannot acquire next swap chain texture");
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
