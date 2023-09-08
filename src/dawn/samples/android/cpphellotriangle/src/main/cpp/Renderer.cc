//
// Created by nexa on 8/18/23.
//

#include "Renderer.h"
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <webgpu/webgpu_cpp.h>
#include "Utilities.cc"

void Renderer::initRenderer() {
    mApp->textInputState = 0;
    androidDesc.window = mApp->window;
    androidDesc.sType = wgpu::SType::SurfaceDescriptorFromAndroidNativeWindow;

    wgpu::SurfaceDescriptor surfaceDesc = {};
    surfaceDesc.nextInChain = static_cast<const wgpu::ChainedStruct*>(&androidDesc);
    wgpu::Surface surface = instance.CreateSurface(&surfaceDesc);

    adapterOpts.backendType = wgpu::BackendType::Vulkan;
    adapterOpts.compatibleSurface = surface;
    adapter = requestAdapter(instance, &adapterOpts);
    deviceDesc.label = "Test Device";
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredLimits = nullptr;
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "The default queue";
    device = requestDevice(adapter, &deviceDesc);
    queue = device.GetQueue();

    auto onDeviceError = [](WGPUErrorType type, char const* message, void* /* pUserData */) {
        LOGI("Uncaptured device error:...");
        if (message) {
            LOGI("%s\n", message);
        }
    };
    device.SetUncapturedErrorCallback(onDeviceError, nullptr /* pUserData */);

    LOGI("Creating swapchain device...");

    wgpu::SwapChainDescriptor swapChainDesc = {};
    swapChainDesc.nextInChain = nullptr;
    swapChainDesc.width = 640;
    swapChainDesc.height = 480;
    swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;

    // The swap chain textures use the color format suggested by the target
    // surface. Currently, dawn accepts only RGBA8Unorm for Android.
    wgpu::TextureFormat swapChainFormat = wgpu::TextureFormat::RGBA8Unorm;
    swapChainDesc.format = swapChainFormat;
    swapChainDesc.presentMode = wgpu::PresentMode::Fifo;
    swapChain = device.CreateSwapChain(surface, &swapChainDesc);

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

    wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc = {};
    shaderCodeDesc.code = shaderSource;
    wgpu::ShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = &shaderCodeDesc;
    wgpu::ShaderModule shaderModule = device.CreateShaderModule(&shaderDesc);
    wgpu::RenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.nextInChain = nullptr;

    // Vertex fetch
    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;

    // Vertex shader
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;
    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
    pipelineDesc.primitive.cullMode = wgpu::CullMode::None;

    // Fragment shader
    wgpu::FragmentState fragmentState = {};
    fragmentState.nextInChain = nullptr;
    pipelineDesc.fragment = &fragmentState;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    // Configure blend state
    wgpu::BlendState blendState;
    blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = wgpu::BlendOperation::Add;
    blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blendState.alpha.dstFactor = wgpu::BlendFactor::One;
    blendState.alpha.operation = wgpu::BlendOperation::Add;

    wgpu::ColorTargetState colorTarget = {};
    colorTarget.nextInChain = nullptr;
    colorTarget.format = swapChainFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    pipelineDesc.depthStencil = nullptr;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    pipelineDesc.layout = nullptr;
    pipeline = device.CreateRenderPipeline(&pipelineDesc);
}

void Renderer::GameLoop() {
    if (!deviceInitialised) {
        initRenderer();
        deviceInitialised = true;
    }

    wgpu::TextureView nextTexture = swapChain.GetCurrentTextureView();
    if (!nextTexture) {
        LOGI("Cannot acquire next swap chain texture");
        return;
    }

    wgpu::RenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = nextTexture;
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
    renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
    renderPassColorAttachment.clearValue = wgpu::Color{0.9, 0.1, 0.2, 1.0};

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.nextInChain = nullptr;
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;

    wgpu::CommandEncoderDescriptor commandEncoderDesc = {};
    commandEncoderDesc.nextInChain = nullptr;
    commandEncoderDesc.label = "Command Encoder";
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder(&commandEncoderDesc);
    wgpu::RenderPassEncoder renderPass = encoder.BeginRenderPass(&renderPassDesc);

    renderPass.SetPipeline(pipeline);
    renderPass.Draw(3, 1, 0, 0);
    renderPass.End();

    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
    swapChain.Present();
}
