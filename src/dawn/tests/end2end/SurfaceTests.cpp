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

class SurfaceTests : public DawnTest {
  public:
    void SetUp() override {
        DawnTest::SetUp();
        DAWN_TEST_UNSUPPORTED_IF(UsesWire());

        glfwSetErrorCallback([](int code, const char* message) {
            ErrorLog() << "GLFW error " << code << " " << message;
        });

        // GLFW can fail to start in headless environments, in which SurfaceTests are
        // inapplicable. Skip this cases without producing a test failure.
        if (glfwInit() == GLFW_FALSE) {
            GTEST_SKIP();
        }

        // Set GLFW_NO_API to avoid GLFW bringing up a GL context that we won't use.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window.reset(glfwCreateWindow(400, 500, "SurfaceTests window", nullptr, nullptr));

        int width;
        int height;
        glfwGetFramebufferSize(window.get(), &width, &height);

        baseConfig.device = device;
        baseConfig.width = 500;
        baseConfig.height = 400;
        baseConfig.usage = wgpu::TextureUsage::RenderAttachment;
    }

    void TearDown() override {
        // Destroy the surface before the window as required by webgpu-native.
        window.reset();
        DawnTest::TearDown();
    }

    wgpu::Surface CreateTestSurface() {
        return wgpu::glfw::CreateSurfaceForWindow(GetInstance(), window.get());
    }

    wgpu::SurfaceConfiguration GetPreferredConfiguration(wgpu::Surface surface) {
        wgpu::SurfaceCapabilities capabilities;
        surface.GetCapabilities(adapter, &capabilities);

        wgpu::TextureFormat preferredFormat = surface.GetPreferredFormat(adapter);

        wgpu::SurfaceConfiguration config = baseConfig;
        config.format = preferredFormat;
        config.alphaMode = capabilities.alphaModes[0];
        config.presentMode = capabilities.presentModes[0];
        config.viewFormatCount = 1;
        config.viewFormats = nullptr; // MUST be filled in by the caller of GetPreferredConfiguration
        return config;
    }

    void ClearTexture(wgpu::Texture texture, wgpu::Color color) {
        utils::ComboRenderPassDescriptor desc({texture.CreateView()});
        desc.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
        desc.cColorAttachments[0].clearValue = color;

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&desc);
        pass.End();

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }

  protected:
    std::unique_ptr<GLFWwindow, GLFWindowDestroyer> window = nullptr;
    wgpu::SurfaceConfiguration baseConfig;
};

// Basic test for creating a swapchain and presenting one frame.
TEST_P(SurfaceTests, Basic) {
    wgpu::Surface surface = CreateTestSurface();

    // Configure
    wgpu::SurfaceConfiguration config = GetPreferredConfiguration(surface);
    config.viewFormats = &config.format;
    surface.Configure(&config);

    // Get texture
    wgpu::SurfaceTexture surfaceTexture;
    surface.GetCurrentTexture(&surfaceTexture);
    ASSERT_EQ(surfaceTexture.status, wgpu::SurfaceGetCurrentTextureStatus::Success);
    ClearTexture(surfaceTexture.texture, { 1.0, 0.0, 0.0, 1.0 });

    // Present
    surface.Present();
}

// Test reconfiguring the surface
TEST_P(SurfaceTests, ReconfigureBasic) {
    wgpu::Surface surface = CreateTestSurface();
    wgpu::SurfaceConfiguration config = GetPreferredConfiguration(surface);
    config.viewFormats = &config.format;

    surface.Configure(&config);

    surface.Configure(&config);
}

// Test replacing the swapchain after GetCurrentTexture
TEST_P(SurfaceTests, ReconfigureAfterGetCurrentTexture) {
    wgpu::Surface surface = CreateTestSurface();
    wgpu::SurfaceConfiguration config = GetPreferredConfiguration(surface);
    config.viewFormats = &config.format;
    wgpu::SurfaceTexture surfaceTexture;

    surface.Configure(&config);
    surface.GetCurrentTexture(&surfaceTexture);
    ClearTexture(surfaceTexture.texture, { 1.0, 0.0, 0.0, 1.0 });

    surface.Configure(&config);
    surface.GetCurrentTexture(&surfaceTexture);
    ClearTexture(surfaceTexture.texture, { 0.0, 1.0, 0.0, 1.0 });
    surface.Present();
}

// Test inconfiguring then reconfiguring the surface
TEST_P(SurfaceTests, ReconfigureAfterUnconfigure) {
    wgpu::Surface surface = CreateTestSurface();
    wgpu::SurfaceConfiguration config = GetPreferredConfiguration(surface);
    config.viewFormats = &config.format;
    wgpu::SurfaceTexture surfaceTexture;

    surface.Configure(&config);
    surface.GetCurrentTexture(&surfaceTexture);
    ClearTexture(surfaceTexture.texture, { 1.0, 0.0, 0.0, 1.0 });
    surface.Present();

    surface.Unconfigure();

    surface.Configure(&config);
    surface.GetCurrentTexture(&surfaceTexture);
    ClearTexture(surfaceTexture.texture, { 0.0, 1.0, 0.0, 1.0 });
    surface.Present();
}

// Test destroying the swapchain after GetCurrentTexture
TEST_P(SurfaceTests, UnconfigureAfterGet) {
    wgpu::Surface surface = CreateTestSurface();
    wgpu::SurfaceConfiguration config = GetPreferredConfiguration(surface);
    config.viewFormats = &config.format;
    wgpu::SurfaceTexture surfaceTexture;

    surface.Configure(&config);
    surface.GetCurrentTexture(&surfaceTexture);
    ClearTexture(surfaceTexture.texture, { 1.0, 0.0, 0.0, 1.0 });

    surface.Unconfigure();
}

#if 0 // TODO(dawn:2320)
// Test switching between present modes.
TEST_P(SurfaceTests, SwitchPresentMode) {
    // Fails with "internal drawable creation failed" on the Windows NVIDIA CQ builders but not
    // locally.
    DAWN_SUPPRESS_TEST_IF(IsWindows() && IsVulkan() && IsNvidia());

    // TODO(jiawei.shao@intel.com): find out why this test sometimes hangs on the latest Linux Intel
    // Vulkan drivers.
    DAWN_SUPPRESS_TEST_IF(IsLinux() && IsVulkan() && IsIntel());

    constexpr wgpu::PresentMode kAllPresentModes[] = {
        wgpu::PresentMode::Immediate,
        wgpu::PresentMode::Fifo,
        wgpu::PresentMode::Mailbox,
    };

    for (wgpu::PresentMode mode1 : kAllPresentModes) {
        for (wgpu::PresentMode mode2 : kAllPresentModes) {
            wgpu::SwapChainDescriptor desc = baseDescriptor;

            desc.presentMode = mode1;
            wgpu::SwapChain swapchain1 = CreateSwapChain(surface, &desc);
            ClearTexture(swapchain1.GetCurrentTexture(), {0.0, 0.0, 0.0, 1.0});
            swapchain1.Present();

            desc.presentMode = mode2;
            wgpu::SwapChain swapchain2 = CreateSwapChain(surface, &desc);
            ClearTexture(swapchain2.GetCurrentTexture(), {0.0, 0.0, 0.0, 1.0});
            swapchain2.Present();
        }
    }
}

// Test resizing the swapchain and without resizing the window.
TEST_P(SurfaceTests, ResizingSwapChainOnly) {
    for (int i = 0; i < 10; i++) {
        wgpu::SwapChainDescriptor desc = baseDescriptor;
        desc.width += i * 10;
        desc.height -= i * 10;

        wgpu::SwapChain swapchain = CreateSwapChain(surface, &desc);
        ClearTexture(swapchain.GetCurrentTexture(), {0.05f * i, 0.0f, 0.0f, 1.0f});
        swapchain.Present();
    }
}

// Test resizing the window but not the swapchain.
TEST_P(SurfaceTests, ResizingWindowOnly) {
    // TODO(crbug.com/1503912): Failing new ValidateImageAcquireWait in Vulkan Validation Layer.
    DAWN_SUPPRESS_TEST_IF(IsBackendValidationEnabled() && IsWindows() && IsVulkan() && IsIntel());

    wgpu::SwapChain swapchain = CreateSwapChain(surface, &baseDescriptor);

    for (int i = 0; i < 10; i++) {
        glfwSetWindowSize(window.get(), 400 - 10 * i, 400 + 10 * i);
        glfwPollEvents();

        ClearTexture(swapchain.GetCurrentTexture(), {0.05f * i, 0.0f, 0.0f, 1.0f});
        swapchain.Present();
    }
}

// Test resizing both the window and the swapchain at the same time.
TEST_P(SurfaceTests, ResizingWindowAndSwapChain) {
    // TODO(crbug.com/dawn/1205) Currently failing on new NVIDIA GTX 1660s on Linux/Vulkan.
    DAWN_SUPPRESS_TEST_IF(IsLinux() && IsVulkan() && IsNvidia());
    for (int i = 0; i < 10; i++) {
        glfwSetWindowSize(window.get(), 400 - 10 * i, 400 + 10 * i);
        glfwPollEvents();

        int width;
        int height;
        glfwGetFramebufferSize(window.get(), &width, &height);

        wgpu::SwapChainDescriptor desc = baseDescriptor;
        desc.width = width;
        desc.height = height;

        wgpu::SwapChain swapchain = CreateSwapChain(surface, &desc);
        ClearTexture(swapchain.GetCurrentTexture(), {0.05f * i, 0.0f, 0.0f, 1.0f});
        swapchain.Present();
    }
}

// Test switching devices on the same adapter.
TEST_P(SurfaceTests, SwitchingDevice) {
    // TODO(https://crbug.com/dawn/2116): Disabled due to new Validation Layer failures.
    DAWN_SUPPRESS_TEST_IF(IsVulkan());

    wgpu::Device device2 = CreateDevice();

    for (int i = 0; i < 3; i++) {
        wgpu::Device deviceToUse;
        if (i % 2 == 0) {
            deviceToUse = device;
        } else {
            deviceToUse = device2;
        }

        wgpu::SwapChain swapchain = deviceToUse.CreateSwapChain(surface, &baseDescriptor);
        swapchain.GetCurrentTexture();
        swapchain.Present();
    }
}

// Test that calling Device.GetSupportedSurfaceUsage() will throw an error because
// SurfaceCapabilities is not enabled.
TEST_P(SurfaceTests, ErrorGetSurfaceSupportedUsage) {
    DAWN_TEST_UNSUPPORTED_IF(HasToggleEnabled("skip_validation"));
    EXPECT_FALSE(device.HasFeature(wgpu::FeatureName::SurfaceCapabilities));

    ASSERT_DEVICE_ERROR_MSG(
        {
            auto usageFlags = device.GetSupportedSurfaceUsage(surface);
            EXPECT_EQ(usageFlags, wgpu::TextureUsage::None);
        },
        testing::HasSubstr("FeatureName::SurfaceCapabilities is not enabled"));
}

// Test that creating swapchain with TextureBinding usage without enabling SurfaceCapabilities
// feature should fail.
TEST_P(SurfaceTests, ErrorCreateWithTextureBindingUsage) {
    DAWN_TEST_UNSUPPORTED_IF(HasToggleEnabled("skip_validation"));
    EXPECT_FALSE(device.HasFeature(wgpu::FeatureName::SurfaceCapabilities));

    auto desc = baseDescriptor;
    desc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment;

    ASSERT_DEVICE_ERROR_MSG(
        { auto swapchain = CreateSwapChain(surface, &desc); },
        testing::HasSubstr("require enabling FeatureName::SurfaceCapabilities"));
}
#endif

DAWN_INSTANTIATE_TEST(SurfaceTests,
                      D3D11Backend(),
                      D3D12Backend(),
                      MetalBackend(),
                      NullBackend(),
                      VulkanBackend());

}  // anonymous namespace
}  // namespace dawn
