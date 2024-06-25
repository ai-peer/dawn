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

#include "dawn/samples/SampleUtils.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "GLFW/glfw3.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "dawn/common/Assert.h"
#include "dawn/common/Log.h"
#include "dawn/common/Platform.h"
#include "dawn/common/SystemUtils.h"
#include "dawn/dawn_proc.h"
#include "dawn/native/DawnNative.h"
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
            DAWN_UNREACHABLE();
    }
    dawn::ErrorLog() << errorTypeName << " error: " << message;
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
            DAWN_UNREACHABLE();
    }
    dawn::ErrorLog() << "Device lost because of " << reasonName << ": message";
}

// Parsed options.
static wgpu::BackendType backendType = wgpu::BackendType::Undefined;
static wgpu::AdapterType adapterType = wgpu::AdapterType::Unknown;
static std::vector<std::string> enableToggles;
static std::vector<std::string> disableToggles;

// Global state
static wgpu::Surface surface;
static wgpu::SwapChain swapChain;
static GLFWwindow* window = nullptr;

static constexpr uint32_t kWidth = 640;
static constexpr uint32_t kHeight = 480;

wgpu::Device CreateCppDawnDevice() {
    dawnProcSetProcs(&dawn::native::GetProcs());

    dawn::ScopedEnvironmentVar angleDefaultPlatform;
    if (dawn::GetEnvironmentVar("ANGLE_DEFAULT_PLATFORM").first.empty()) {
        angleDefaultPlatform.Set("ANGLE_DEFAULT_PLATFORM", "swiftshader");
    }

    glfwSetErrorCallback([](int code, const char* message) {
        dawn::ErrorLog() << "GLFW error: " << code << " - " << message;
    });

    if (!glfwInit()) {
        return wgpu::Device();
    }

    // Create the test window with no client API.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(kWidth, kHeight, "Dawn window", nullptr, nullptr);
    if (!window) {
        return wgpu::Device();
    }

    // Create the instance with the toggles
    std::vector<const char*> enableToggleNames;
    std::vector<const char*> disabledToggleNames;
    for (const std::string& toggle : enableToggles) {
        enableToggleNames.push_back(toggle.c_str());
    }

    for (const std::string& toggle : disableToggles) {
        disabledToggleNames.push_back(toggle.c_str());
    }
    wgpu::DawnTogglesDescriptor toggles;
    toggles.enabledToggles = enableToggleNames.data();
    toggles.enabledToggleCount = enableToggleNames.size();
    toggles.disabledToggles = disabledToggleNames.data();
    toggles.disabledToggleCount = disabledToggleNames.size();

    wgpu::InstanceDescriptor instanceDescriptor{};
    instanceDescriptor.nextInChain = &toggles;
    instanceDescriptor.features.timedWaitAnyEnable = true;
    wgpu::Instance instance = wgpu::CreateInstance(&instanceDescriptor);

    // Synchronously request the adapter.
    wgpu::RequestAdapterOptions options = {};
    options.backendType = backendType;
    switch (adapterType) {
        case wgpu::AdapterType::CPU:
            options.forceFallbackAdapter = true;
            break;
        case wgpu::AdapterType::DiscreteGPU:
            options.powerPreference = wgpu::PowerPreference::HighPerformance;
            break;
        case wgpu::AdapterType::IntegratedGPU:
            options.powerPreference = wgpu::PowerPreference::LowPower;
            break;
        case wgpu::AdapterType::Unknown:
            break;
    }

    wgpu::Adapter adapter;
    wgpu::FutureWaitInfo adapterFuture = {};
    adapterFuture.future = instance.RequestAdapter(
        &options, {nullptr, wgpu::CallbackMode::WaitAnyOnly,
                   [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message,
                      void* userdata) {
                       if (status != WGPURequestAdapterStatus_Success) {
                           dawn::ErrorLog() << "Failed to get an adapter:" << message;
                           return;
                       }
                       *static_cast<wgpu::Adapter*>(userdata) = wgpu::Adapter::Acquire(adapter);
                   },
                   &adapter});
    instance.WaitAny(1, &adapterFuture, UINT64_MAX);
    DAWN_ASSERT(adapterFuture.completed);

    if (adapter == nullptr) {
        return wgpu::Device();
    }

    wgpu::AdapterProperties properties;
    adapter.GetProperties(&properties);
    dawn::InfoLog() << "Using adapter \"" << properties.name << "\"";

    // Synchronously request the device.
    wgpu::DeviceDescriptor deviceDesc;
    deviceDesc.uncapturedErrorCallbackInfo = {nullptr, PrintDeviceError, nullptr};
    deviceDesc.deviceLostCallbackInfo = {nullptr, wgpu::CallbackMode::AllowSpontaneous,
                                         PrintDeviceLoss, nullptr};

    wgpu::Device device;
    wgpu::FutureWaitInfo deviceFuture = {};
    deviceFuture.future = adapter.RequestDevice(
        &deviceDesc, {nullptr, wgpu::CallbackMode::WaitAnyOnly,
                      [](WGPURequestDeviceStatus status, WGPUDevice device, const char* message,
                         void* userdata) {
                          if (status != WGPURequestDeviceStatus_Success) {
                              dawn::ErrorLog() << "Failed to get an device:" << message;
                              return;
                          }
                          *static_cast<wgpu::Device*>(userdata) = wgpu::Device::Acquire(device);
                      },
                      &device});
    instance.WaitAny(1, &deviceFuture, UINT64_MAX);
    DAWN_ASSERT(deviceFuture.completed);

    if (device == nullptr) {
        return wgpu::Device();
    }

    // Create the swapchain
    surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);

    wgpu::SwapChainDescriptor swapChainDesc = {};
    swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
    swapChainDesc.format = GetPreferredSwapChainTextureFormat();
    swapChainDesc.width = kWidth;
    swapChainDesc.height = kHeight;
    swapChainDesc.presentMode = wgpu::PresentMode::Mailbox;
    swapChain = device.CreateSwapChain(surface, &swapChainDesc);

    return device;
}

wgpu::TextureFormat GetPreferredSwapChainTextureFormat() {
    // TODO(dawn:1362): Return the adapter's preferred format when implemented.
    return wgpu::TextureFormat::BGRA8Unorm;
}

wgpu::SwapChain GetSwapChain() {
    return swapChain;
}

namespace wgpu {

std::string AbslUnparseFlag(BackendType b) {
    switch (b) {
        case BackendType::D3D11:
            return "d3d11";
        case BackendType::D3D12:
            return "d3d12";
        case BackendType::Metal:
            return "metal";
        case BackendType::Null:
            return "null";
        case BackendType::OpenGL:
            return "opengl";
        case BackendType::OpenGLES:
            return "opengles";
        case BackendType::Vulkan:
            return "vulkan";
        case BackendType::WebGPU:
            return "webgpu";
        case BackendType::Undefined:
            return "undefined";
    }
}

bool AbslParseFlag(absl::string_view text, BackendType* type, std::string* error) {
    if (text == "d3d11") {
        *type = BackendType::D3D11;
        return true;
    }
    if (text == "d3d12") {
        *type = BackendType::D3D12;
        return true;
    }
    if (text == "metal") {
        *type = BackendType::Metal;
        return true;
    }
    if (text == "null") {
        *type = BackendType::Null;
        return true;
    }
    if (text == "opengl") {
        *type = BackendType::OpenGL;
        return true;
    }
    if (text == "opengles") {
        *type = BackendType::OpenGLES;
        return true;
    }
    if (text == "vulkan") {
        *type = BackendType::Vulkan;
        return true;
    }
    if (text == "webgpu") {
        *type = BackendType::WebGPU;
        return true;
    }

    *error = "expected one of d3d11, d3d12, metal, null, opengl, opengles, vulkan, webgpu";
    return false;
}

std::string AbslUnparseFlag(AdapterType t) {
    switch (t) {
        case AdapterType::DiscreteGPU:
            return "discrete";
        case AdapterType::IntegratedGPU:
            return "integrated";
        case AdapterType::CPU:
            return "CPU";
        case AdapterType::Unknown:
            return "unknown";
    }
}

bool AbslParseFlag(absl::string_view text, AdapterType* type, std::string* error) {
    if (text == "discrete") {
        *type = AdapterType::DiscreteGPU;
        return true;
    }
    if (text == "integrated") {
        *type = AdapterType::IntegratedGPU;
        return true;
    }
    if (text == "CPU") {
        *type = AdapterType::CPU;
        return true;
    }

    *error = "expected one of discrete, integrated, cpu";
    return false;
}

}  // namespace wgpu

ABSL_FLAG(std::vector<std::string>,
          enable_toggles,
          {},
          "comma=separated list of toggles to enable");
ABSL_FLAG(std::vector<std::string>,
          disable_toggles,
          {},
          "comma=separated list of toggles to enable");
ABSL_FLAG(wgpu::BackendType,
          backend,
          wgpu::BackendType::Undefined,
          "the backend to get an adapter from");
ABSL_FLAG(wgpu::AdapterType,
          adapter_type,
          wgpu::AdapterType::Unknown,
          "the type of adapter to request");

bool InitSample(int argc, char** argv) {
    absl::ParseCommandLine(argc, argv);

    backendType = absl::GetFlag(FLAGS_backend);
    adapterType = absl::GetFlag(FLAGS_adapter_type);
    enableToggles = absl::GetFlag(FLAGS_enable_toggles);
    disableToggles = absl::GetFlag(FLAGS_disable_toggles);

    // TODO(dawn:810): Reenable once the OpenGL(ES) backend is able to create its own context such
    // that it can use surface-based swapchains.
    if (backendType == wgpu::BackendType::OpenGL || backendType == wgpu::BackendType::OpenGLES) {
        fprintf(stderr,
                "The OpenGL(ES) backend is temporarily not supported for samples. See "
                "https://crbug.com/dawn/810\n");
        return false;
    }

    return true;
}

void DoFlush() {
    glfwPollEvents();
}

bool ShouldQuit() {
    return glfwWindowShouldClose(window);
}

GLFWwindow* GetGLFWWindow() {
    return window;
}
