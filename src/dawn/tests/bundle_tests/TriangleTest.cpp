// Copyright 2017 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * This is a more extensive test which draws a red triangle
 * using the monolithic shared library webgpu_dawn and dawn_glfw
 */

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "GLFW/glfw3.h"

#include "dawn/webgpu_cpp_print.h"
#include "webgpu/webgpu_cpp.h"
#include "webgpu/webgpu_glfw.h"

void PrintDeviceError(WGPUErrorType errorType, const char* message, void*) {
    const char* errorTypeName = "";
    switch (errorType) {
        case WGPUErrorType_Validation:
            errorTypeName = "Validation";
            break;
        case WGPUErrorType_OutOfMemory:
            errorTypeName = "Out of memory";
            break;
        case WGPUErrorType_Unknown:
            errorTypeName = "Unknown";
            break;
        case WGPUErrorType_DeviceLost:
            errorTypeName = "Device lost";
            break;
        default:
            break;
    }
    std::cerr << errorTypeName << " error: " << message << '\n';
}

void PrintDeviceLoss(const WGPUDevice* device,
                     WGPUDeviceLostReason reason,
                     const char* message,
                     void* userdata) {
    const char* reasonName = "";
    switch (reason) {
        case WGPUDeviceLostReason_Unknown:
            reasonName = "Unknown";
            break;
        case WGPUDeviceLostReason_Destroyed:
            reasonName = "Destroyed";
            break;
        case WGPUDeviceLostReason_InstanceDropped:
            reasonName = "InstanceDropped";
            break;
        case WGPUDeviceLostReason_FailedCreation:
            reasonName = "FailedCreation";
            break;
        default:
            break;
    }
    std::cerr << "Device lost because of " << reasonName << ": message\n";
}

std::string AsHex(uint32_t val) {
    std::stringstream hex;
    hex << "0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << val;
    return hex.str();
}

std::string AdapterPropertiesToString(const wgpu::AdapterProperties& props) {
    std::stringstream out;
    out << "VendorID: " << AsHex(props.vendorID) << "\n";
    out << "Vendor: " << props.vendorName << "\n";
    out << "Architecture: " << props.architecture << "\n";
    out << "DeviceID: " << AsHex(props.deviceID) << "\n";
    out << "Name: " << props.name << "\n";
    out << "Driver description: " << props.driverDescription << "\n";
    out << "Adapter Type: " << props.adapterType << "\n";
    out << "Backend Type: " << props.backendType << "\n";

    return out.str();
}

std::string PowerPreferenceToString(const wgpu::DawnAdapterPropertiesPowerPreference& prop) {
    switch (prop.powerPreference) {
        case wgpu::PowerPreference::LowPower:
            return "low power";
        case wgpu::PowerPreference::HighPerformance:
            return "high performance";
        case wgpu::PowerPreference::Undefined:
            return "<undefined>";
    }
    return "<unknown>";
}

void DumpAdapterProperties(const wgpu::Adapter& adapter) {
    wgpu::DawnAdapterPropertiesPowerPreference power_props{};

    wgpu::AdapterProperties properties{};
    properties.nextInChain = &power_props;

    adapter.GetProperties(&properties);
    std::cout << AdapterPropertiesToString(properties);
    std::cout << "Power: " << PowerPreferenceToString(power_props) << "\n";
    std::cout << "\n";
}

void DumpAdapter(const wgpu::Adapter& adapter) {
    std::cout << "Adapter\n";
    std::cout << "=======\n";

    DumpAdapterProperties(adapter);
}

int main() {
    wgpu::InstanceDescriptor instanceDescriptor{};
    instanceDescriptor.features.timedWaitAnyEnable = true;
    wgpu::Instance instance = wgpu::CreateInstance(&instanceDescriptor);
    if (instance == nullptr) {
        std::cerr << "ERROR: Failed to create instance!\n";
        return EXIT_FAILURE;
    }

    // Open window
    if (!glfwInit()) {
        std::cerr << "ERROR: Failed to initialize glfw!\n";
        return EXIT_FAILURE;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);
    wgpu::Surface surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);

    // Synchronously request the adapter.
    wgpu::RequestAdapterOptions options = {};
    options.powerPreference = wgpu::PowerPreference::HighPerformance;
    options.compatibleSurface = surface;
    wgpu::Adapter adapter;
    instance.WaitAny(instance.RequestAdapter(
                         &options, {nullptr, wgpu::CallbackMode::WaitAnyOnly,
                                    [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                                       const char* message, void* userdata) {
                                        if (status != WGPURequestAdapterStatus_Success) {
                                            std::cerr << "Failed to get an adapter:" << message;
                                            return;
                                        }
                                        *static_cast<wgpu::Adapter*>(userdata) =
                                            wgpu::Adapter::Acquire(adapter);
                                    },
                                    &adapter}),
                     UINT64_MAX);
    if (adapter == nullptr) {
        std::cerr << "ERROR: Failed to request an adapter!\n";
        return EXIT_FAILURE;
    }
    DumpAdapter(adapter);

    // Synchronously request the device.
    wgpu::DeviceDescriptor deviceDesc;
    deviceDesc.uncapturedErrorCallbackInfo = {nullptr, PrintDeviceError, nullptr};
    deviceDesc.deviceLostCallbackInfo = {nullptr, wgpu::CallbackMode::AllowSpontaneous,
                                         PrintDeviceLoss, nullptr};
    wgpu::Device device;
    instance.WaitAny(adapter.RequestDevice(
                         &deviceDesc, {nullptr, wgpu::CallbackMode::WaitAnyOnly,
                                       [](WGPURequestDeviceStatus status, WGPUDevice device,
                                          const char* message, void* userdata) {
                                           if (status != WGPURequestDeviceStatus_Success) {
                                               std::cerr << "Failed to get an device:" << message;
                                               return;
                                           }
                                           *static_cast<wgpu::Device*>(userdata) =
                                               wgpu::Device::Acquire(device);
                                       },
                                       &device}),
                     UINT64_MAX);

    if (device == nullptr) {
        std::cerr << "ERROR: Failed to request a device!\n";
        return EXIT_FAILURE;
    }
    // Configure the surface
    wgpu::SurfaceConfiguration config = {};
    config.nextInChain = nullptr;

    // Configuration of the textures created for the underlying swap chain
    config.width = 640;
    config.height = 480;
    config.usage = wgpu::TextureUsage::RenderAttachment;
    wgpu::SurfaceCapabilities caps = {};
    surface.GetCapabilities(adapter, &caps);
    config.format = caps.formats[0];

    // And we do not need any particular view format:
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.device = device;
    config.presentMode = wgpu::PresentMode::Fifo;
    config.alphaMode = wgpu::CompositeAlphaMode::Auto;

    surface.Configure(&config);

    const char shaderCode[] = R"(
    @vertex fn vertexMain(@builtin(vertex_index) i : u32) ->
      @builtin(position) vec4f {
        const pos = array(vec2f(0, 1), vec2f(-1, -1), vec2f(1, -1));
        return vec4f(pos[i], 0, 1);
    }
    @fragment fn fragmentMain() -> @location(0) vec4f {
        return vec4f(1, 0, 0, 1);
    }
)";

    wgpu::ShaderModuleWGSLDescriptor wgslDesc = {};
    wgslDesc.code = shaderCode;

    wgpu::ShaderModuleDescriptor shaderModuleDescriptor;
    shaderModuleDescriptor.nextInChain = &wgslDesc;
    wgpu::ShaderModule shaderModule = device.CreateShaderModule(&shaderModuleDescriptor);

    wgpu::ColorTargetState colorTargetState = {};
    colorTargetState.format = caps.formats[0];

    wgpu::FragmentState fragmentState = {};
    fragmentState.module = shaderModule;
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTargetState;

    wgpu::VertexState vertexState = {};
    vertexState.module = shaderModule;
    wgpu::RenderPipelineDescriptor pipelineDescriptor = {};
    pipelineDescriptor.vertex = vertexState;
    pipelineDescriptor.fragment = &fragmentState;
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&pipelineDescriptor);

    wgpu::Queue queue = device.GetQueue();

    // Get the next target texture view
    // Get the surface texture
    wgpu::SurfaceTexture surfaceTexture;
    surface.GetCurrentTexture(&surfaceTexture);
    if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::Success) {
        return EXIT_FAILURE;
    }

    // Create a view for this surface texture
    wgpu::TextureViewDescriptor viewDescriptor = {};
    viewDescriptor.format = surfaceTexture.texture.GetFormat();
    viewDescriptor.dimension = wgpu::TextureViewDimension::e2D;
    viewDescriptor.baseMipLevel = 0;
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.baseArrayLayer = 0;
    viewDescriptor.arrayLayerCount = 1;
    viewDescriptor.aspect = wgpu::TextureAspect::All;
    wgpu::TextureView targetView = surfaceTexture.texture.CreateView(&viewDescriptor);
    if (!targetView) {
        return EXIT_FAILURE;
    }

    // Create a command encoder for the draw call
    wgpu::CommandEncoderDescriptor encoderDesc = {};
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder(&encoderDesc);

    // Create the render pass that clears the screen with our color
    wgpu::RenderPassDescriptor renderPassDesc = {};

    // The attachment part of the render pass descriptor describes the target
    // texture of the pass
    wgpu::RenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = targetView;
    renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
    renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
    renderPassColorAttachment.clearValue = wgpu::Color{0.0, 0.1, 0.2, 1.0};

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;

    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
        pass.SetPipeline(pipeline);
        pass.Draw(3);
        pass.End();
    }

    // Finally encode and submit the render pass
    wgpu::CommandBufferDescriptor cmdBufferDescriptor = {};
    wgpu::CommandBuffer command = encoder.Finish(&cmdBufferDescriptor);

    std::cout << "Submitting command..." << std::endl;
    queue.Submit(1, &command);
    std::cout << "Command submitted." << std::endl;

    surface.Present();
    device.Tick();
    return EXIT_SUCCESS;
}
