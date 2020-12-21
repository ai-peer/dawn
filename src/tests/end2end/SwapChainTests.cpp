// Copyright 2020 The Dawn Authors
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

#include "tests/DawnTest.h"

#include "common/Constants.h"
#include "common/Log.h"
#include "utils/GLFWUtils.h"
#include "utils/WGPUHelpers.h"

#include "GLFW/glfw3.h"

class SwapChainTests : public DawnTest {
  public:
    void SetUp() override {
        DawnTest::SetUp();
        DAWN_SKIP_TEST_IF(UsesWire());

        glfwSetErrorCallback([](int code, const char* message) {
            dawn::ErrorLog() << "GLFW error " << code << " " << message;
        });

        // GLFW can fail to start in headless environments, in which SwapChainTests are
        // inapplicable. Skip this cases without producing a test failure.
        if (glfwInit() == GLFW_FALSE) {
            GTEST_SKIP();
        }

        // The SwapChainTests don't create OpenGL contexts so we don't need to call
        // SetupGLFWWindowHintsForBackend. Set GLFW_NO_API anyway to avoid GLFW bringing up a GL
        // context that we won't use.
        ASSERT_TRUE(!IsOpenGL());
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(400, 400, "SwapChainValidationTests window", nullptr, nullptr);

        int width;
        int height;
        glfwGetFramebufferSize(window, &width, &height);

        surface = utils::CreateSurfaceForWindow(GetInstance(), window);
        ASSERT_NE(surface, nullptr);

        baseDescriptor.width = width;
        baseDescriptor.height = height;
        baseDescriptor.usage = wgpu::TextureUsage::RenderAttachment;
        baseDescriptor.format = wgpu::TextureFormat::BGRA8Unorm;
        baseDescriptor.presentMode = wgpu::PresentMode::Mailbox;
    }

    void TearDown() override {
        // Destroy the surface before the window as required by webgpu-native.
        surface = wgpu::Surface();
        if (window != nullptr) {
            glfwDestroyWindow(window);
        }
        DawnTest::TearDown();
    }

    void ClearTexture(wgpu::TextureView view, wgpu::Color color) {
        utils::ComboRenderPassDescriptor desc({view});
        desc.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
        desc.cColorAttachments[0].clearColor = color;

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&desc);
        pass.EndPass();

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }

    void TestPresentMode(wgpu::PresentMode mode1, wgpu::PresentMode mode2) {
        printf("mode1: %d, mode2: %d\n", mode1, mode2);
        wgpu::SwapChainDescriptor desc = baseDescriptor;

        desc.presentMode = mode1;
        wgpu::SwapChain swapchain1 = device.CreateSwapChain(surface, &desc);
        ClearTexture(swapchain1.GetCurrentTextureView(), {0.0, 0.0, 0.0, 1.0});
        printf("Cleared swapchain1\n");
        swapchain1.Present();
        printf("Presented swapchain1\n");

        desc.presentMode = mode2;
        wgpu::SwapChain swapchain2 = device.CreateSwapChain(surface, &desc);
        ClearTexture(swapchain2.GetCurrentTextureView(), {0.0, 0.0, 0.0, 1.0});
        printf("Cleared swapchain2\n");
        swapchain2.Present();
        printf("Presented swapchain2\n");
    }

  protected:
    GLFWwindow* window = nullptr;
    wgpu::Surface surface;

    wgpu::SwapChainDescriptor baseDescriptor;
};



// Test switching between present modes.
TEST_P(SwapChainTests, SwitchPresentMode) {
    TestPresentMode(wgpu::PresentMode::Mailbox, wgpu::PresentMode::Fifo);
}


DAWN_INSTANTIATE_TEST(SwapChainTests, MetalBackend(), VulkanBackend());
