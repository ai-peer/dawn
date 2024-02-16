// Copyright 2024 The Dawn & Tint Authors
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

#include <cmath>
#include <string>
#include <vector>

#include "dawn/common/Constants.h"
#include "dawn/tests/DawnTest.h"
#include "dawn/utils/ComboRenderBundleEncoderDescriptor.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"
#include "webgpu/webgpu_glfw.h"

#include "GLFW/glfw3.h"

namespace dawn {
namespace {

struct GLFWindowDestroyer {
    void operator()(GLFWwindow* ptr) { glfwDestroyWindow(ptr); }
};

class SurfaceConfigurationValidationTest : public DawnTest {
  public:
    void SetUp() override {
        DawnTest::SetUp();
        DAWN_TEST_UNSUPPORTED_IF(UsesWire());
        DAWN_TEST_UNSUPPORTED_IF(HasToggleEnabled("skip_validation"));

        glfwSetErrorCallback([](int code, const char* message) {
            ErrorLog() << "GLFW error " << code << " " << message;
        });

        // GLFW can fail to start in headless environments, in which SwapChainTests are
        // inapplicable. Skip this cases without producing a test failure.
        if (glfwInit() == GLFW_FALSE) {
            GTEST_SKIP();
        }

        // Set GLFW_NO_API to avoid GLFW bringing up a GL context that we won't use.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window.reset(glfwCreateWindow(500, 400, "SurfaceConfigurationValidationTests window", nullptr, nullptr));

        int width;
        int height;
        glfwGetFramebufferSize(window.get(), &width, &height);
    }

    wgpu::Surface CreateTestSurface() {
        return wgpu::glfw::CreateSurfaceForWindow(GetInstance(), window.get());
    }

private:
    std::unique_ptr<GLFWwindow, GLFWindowDestroyer> window = nullptr;
};

// Using undefined format is not valid
TEST_P(SurfaceConfigurationValidationTest, UndefinedFormat) {
    wgpu::SurfaceConfiguration config;
    config.device = device;
    config.format = wgpu::TextureFormat::Undefined;
    ASSERT_DEVICE_ERROR(CreateTestSurface().Configure(&config));
}

// Supports at least one configuration
TEST_P(SurfaceConfigurationValidationTest, AtLeastOneSupportedConfiguration) {
    wgpu::Surface surface = CreateTestSurface();

    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(adapter, &capabilities);

    ASSERT_GT(capabilities.formatCount, 0);
    ASSERT_GT(capabilities.alphaModeCount, 0);
    ASSERT_GT(capabilities.presentModeCount, 0);
}

// Using any combination of the reported capability is ok for configuring the surface.
TEST_P(SurfaceConfigurationValidationTest, AnyCombinationOfCapabilities) {
    wgpu::Surface surface = CreateTestSurface();

    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(adapter, &capabilities);

    wgpu::SurfaceConfiguration config;
    config.device = device;
    config.width = 500;
    config.height = 400;
    config.usage = wgpu::TextureUsage::RenderAttachment;
    config.viewFormatCount = 1;

    // tmp
    device.SetDeviceLostCallback(
        [](WGPUDeviceLostReason reason, char const* message, void* userdata) {
            std::cerr << "Device Lost! Reason = " << reason;
            if (message) std::cerr << ", message = " << message << std::endl;
            std::cerr << std::endl;
        },
        nullptr
    );

    for (size_t formatIdx = 0; formatIdx < capabilities.formatCount; ++formatIdx) {
        for (size_t alphaModeIdx = 0; alphaModeIdx < capabilities.alphaModeCount; ++alphaModeIdx) {
            for (size_t presentModeIdx = 0; presentModeIdx < capabilities.presentModeCount; ++presentModeIdx) {

                config.format = capabilities.formats[formatIdx];
                config.alphaMode = capabilities.alphaModes[alphaModeIdx];
                config.presentMode = capabilities.presentModes[presentModeIdx];
                config.viewFormats = &capabilities.formats[formatIdx];
                surface.Configure(&config);
                
                // Check that we can present
                wgpu::SurfaceTexture surfaceTexture;
                surface.GetCurrentTexture(&surfaceTexture);
                surface.Present();
                device.Tick();
            }
        }
    }
}

// Preferred format is always valid.
TEST_P(SurfaceConfigurationValidationTest, PreferredFormatIsValid) {
    wgpu::Surface surface = CreateTestSurface();

    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(adapter, &capabilities);

    wgpu::TextureFormat preferredFormat = surface.GetPreferredFormat(adapter);
    bool found = false;
    for (size_t i = 0; i < capabilities.formatCount; ++i) {
        found = found || capabilities.formats[i] == preferredFormat;
    }
    ASSERT_TRUE(found);
}

// A surface that was not configured must not be unconfigured
TEST_P(SurfaceConfigurationValidationTest, UnconfigureNonConfiguredSurfaceFails) {
    // TODO(dawn:2320) This cannot throw a device error since the surface is
    // not aware of the device at this stage.
    /*ASSERT_DEVICE_ERROR(*/CreateTestSurface().Unconfigure()/*)*/;
}

DAWN_INSTANTIATE_TEST(SurfaceConfigurationValidationTest,
                      D3D11Backend(),
                      D3D12Backend(),
                      MetalBackend(),
                      NullBackend(),
                      VulkanBackend());

}  // anonymous namespace
}  // namespace dawn
