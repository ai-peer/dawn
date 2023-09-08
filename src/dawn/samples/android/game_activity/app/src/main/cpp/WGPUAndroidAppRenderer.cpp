#include <dawn/webgpu.h>
#include <iostream>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android/log.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <cassert>
#include <vector>

#define LOG_TAG "gameactivity-webgpu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG  , LOG_TAG,__VA_ARGS__)

WGPUAdapter requestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const * options);
WGPUDevice requestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor);

class WGPUAndroidAppRenderer {
 public:
  android_app *mApp;
  WGPUInstanceDescriptor desc;
  WGPUInstance instance;
  ANativeWindow* nativeWindow;
  WGPURequestAdapterOptions adapterOpts;
  WGPUAdapter adapter;
  WGPUDevice device;
  WGPUDeviceDescriptor deviceDesc;
  WGPUSurfaceDescriptorFromAndroidNativeWindow androidDesc;
  WGPUSwapChain swapChain;
  WGPURenderPipeline pipeline;
  WGPUQueue queue;
  bool deviceInitialised=  false;


  WGPUAndroidAppRenderer(android_app *app) {
    // 1. We create a descriptor
    mApp = app;
    desc = {};
    desc.nextInChain = nullptr;
    // 2. We create the instance using this descriptor
    instance = wgpuCreateInstance(&desc);

    // 3. We can check whether there is actually an instance created
    if (!instance) {
      std::cerr << "Could not initialize WebGPU!" << std::endl;
      return;
    }

    //Get Adapter
    LOGI("Requesting adapter.. ");
    adapterOpts = {};
    adapterOpts.nextInChain = nullptr;

    androidDesc = {};
    deviceDesc = {};
    deviceDesc.nextInChain = nullptr;
  }

  ~WGPUAndroidAppRenderer() {
    // Clean up used resources
    wgpuSwapChainRelease(swapChain);
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(adapter);
    wgpuInstanceRelease(instance);
  }

  void initRenderer() {
    mApp->textInputState = 0;

    // TODO: Get window for Android
    androidDesc.window = mApp->window;
    androidDesc.chain.sType = WGPUSType_SurfaceDescriptorFromAndroidNativeWindow;

    WGPUSurfaceDescriptor surfaceDesc = {};
    surfaceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&androidDesc);
    WGPUSurface surface = wgpuInstanceCreateSurface(instance, &surfaceDesc);

    LOGI("Requesting adapter...");
    adapterOpts.backendType = WGPUBackendType_Vulkan;
    adapterOpts.compatibleSurface = surface;
    adapter = requestAdapter(instance, &adapterOpts);

    // Get Device
    LOGI("Requesting device...");
    deviceDesc.label = "Test Device";
    deviceDesc.requiredFeatureCount = 0; // we do not require any specific feature
    deviceDesc.requiredLimits = nullptr; // we do not require any specific limit
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "The default queue";
    // Build device descriptor
    device = requestDevice(adapter, &deviceDesc);

//    std::cout << "Got device: " << device << std::endl;

    LOGI("Setting the queue...");
    // Get the main and only command queue used to send instructions to the GPU
    queue = wgpuDeviceGetQueue(device);

    // Add a callback that gets executed upon errors in our use of the device
    // By default Dawn runs callbacks only when the device “ticks”, so the error
    // callbacks are invoked in a different call stack than where the error occurred.
    // See https://eliemichel.github.io/LearnWebGPU/getting-started/the-device.html to force invoke

    auto onDeviceError =
        [](WGPUErrorType type, char const* message, void* /* pUserData */) {
          LOGI("Uncaptured device error:...");
          std::cout << "Uncaptured device error: type " << type;
          if (message) std::cout << " (" << message << ")";
          std::cout << std::endl;
        };
    wgpuDeviceSetUncapturedErrorCallback(device,
                                         onDeviceError,
                                         nullptr /* pUserData */);

    LOGI("Creating swapchain device..." );

    // We describe the Swap Chain that is used to present rendered textures on
    // screen. Note that it is specific to a given window size so don't resize.

    WGPUSwapChainDescriptor swapChainDesc = {};
    swapChainDesc.nextInChain = nullptr;
    swapChainDesc.width = 640;
    swapChainDesc.height = 480;

    // Like buffers, textures are allocated for a specific usage. In our case,
    // we will use them as the target of a Render Pass so it needs to be created
    // with the `RenderAttachment` usage flag.
    swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;

    // The swap chain textures use the color format suggested by the target surface.
    LOGI("Creating texture format..." );
    WGPUTextureFormat swapChainFormat = WGPUTextureFormat_RGBA8Unorm;
    swapChainDesc.format = swapChainFormat;

    // FIFO stands for "first in, first out", meaning that the presented
    // texture is always the oldest one, like a regular queue.
    swapChainDesc.presentMode = WGPUPresentMode_Fifo;

    // Finally create the Swap Chain
    // TODO: Get the surface here for Android
    swapChain = wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);

    std::cout << "Swapchain: " << swapChain << std::endl;
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

    // Use the extension mechanism to load a WGSL shader source code
    WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {};
    // Set the chained struct's header
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;

    // Setup the actual payload of the shader code descriptor
    shaderCodeDesc.code = shaderSource;

    WGPUShaderModuleDescriptor shaderDesc = {};
    // Connect the chain
    shaderDesc.nextInChain = &shaderCodeDesc.chain;


    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);

    LOGI("Creating render pipeline..." );
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.nextInChain = nullptr;

    // Vertex fetch
    // (We don't use any input buffer so far)
    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;

    // Vertex shader
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;

    // Primitive assembly and rasterization
    // Each sequence of 3 vertices is considered as a triangle
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    // We'll see later how to specify the order in which vertices should be
    // connected. When not specified, vertices are considered sequentially.
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    // The face orientation is defined by assuming that when looking
    // from the front of the face, its corner vertices are enumerated
    // in the counter-clockwise (CCW) order.
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    // But the face orientation does not matter much because we do not
    // cull (i.e. "hide") the faces pointing away from us (which is often
    // used for optimization).
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
    // Usual alpha blending for the color:
    blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.color.operation = WGPUBlendOperation_Add;
    // We leave the target alpha untouched:
    blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
    blendState.alpha.dstFactor = WGPUBlendFactor_One;
    blendState.alpha.operation = WGPUBlendOperation_Add;

    WGPUColorTargetState colorTarget = {};
    colorTarget.nextInChain = nullptr;
    colorTarget.format = swapChainFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = WGPUColorWriteMask_All; // We could write to only some of the color channels.

    // We have only one target because our render pass has only one output color
    // attachment.
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    // Depth and stencil tests are not used here
    pipelineDesc.depthStencil = nullptr;

    // Multi-sampling
    // Samples per pixel
    pipelineDesc.multisample.count = 1;
    // Default value for the mask, meaning "all bits on"
    pipelineDesc.multisample.mask = ~0u;
    // Default value as well (irrelevant for count = 1 anyways)
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    // Pipeline layout
    pipelineDesc.layout = nullptr;

    pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
    LOGI( "Render pipeline: " );
  }

  void GameLoop() {
    if(!deviceInitialised) {
      initRenderer();
      deviceInitialised = true;
    }

    // TODO: Add "poll events" check

    LOGI("In the Game Loop");
    while (mApp->window) {
      // TODO: While the window is open
      // Get the texture where to draw the next frame
      WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(swapChain);

      // Getting the texture may fail, in particular if the window has been resized
      // and thus the target surface changed.
      if (!nextTexture) {
        LOGI("Cannot acquire next swap chain texture" );
        break;
      }
      std::cout << "nextTexture: " << nextTexture << std::endl;

      WGPURenderPassColorAttachment renderPassColorAttachment = {};
      // The attachment is tied to the view returned by the swap chain, so that
      // the render pass draws directly on screen.
      renderPassColorAttachment.view = nextTexture;
      // Not relevant here because we do not use multi-sampling
      renderPassColorAttachment.resolveTarget = nullptr;
      renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
      renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
      renderPassColorAttachment.clearValue = WGPUColor{ 0.9, 0.1, 0.2, 1.0 };

      // Describe a render pass, which targets the texture view
      WGPURenderPassDescriptor renderPassDesc = {};
      renderPassDesc.nextInChain = nullptr;
      renderPassDesc.colorAttachmentCount = 1;
      renderPassDesc.colorAttachments = &renderPassColorAttachment;


//      // No depth buffer for now
//      renderPassDesc.depthStencilAttachment = nullptr;
//
//      // We do not use timers for now neither
//      renderPassDesc.timestampWriteCount = 0;
//      renderPassDesc.timestampWrites = nullptr;
//
//      renderPassDesc.nextInChain = nullptr;



      WGPUCommandEncoderDescriptor commandEncoderDesc = {};
      commandEncoderDesc.nextInChain = nullptr;
      commandEncoderDesc.label = "Command Encoder";
      WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &commandEncoderDesc);
      // Create a render pass. We end it immediately because we use its built-in
      // mechanism for clearing the screen when it begins (see descriptor).
      WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
      // In its overall outline, drawing a triangle is as simple as this:
      // Select which render pipeline to use
//      wgpuCommandEncoderClearBuffer(encoder,
      wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);
      // Draw 1 instance of a 3-vertices shape
      wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);
      wgpuRenderPassEncoderEnd(renderPass);

      wgpuTextureViewRelease(nextTexture);

      WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
      cmdBufferDescriptor.nextInChain = nullptr;
      cmdBufferDescriptor.label = "Command buffer";
      WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
      wgpuQueueSubmit(queue, 1, &command);

      // We can tell the swap chain to present the next texture.
      wgpuSwapChainPresent(swapChain);
      break;
    }
  }
};