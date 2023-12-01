// Copyright 2023 The Dawn & Tint Authors
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

#include <android/hardware_buffer.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "dawn/tests/end2end/SharedTextureMemoryTests.h"
#include "dawn/webgpu_cpp.h"

namespace dawn {
namespace {

class Backend : public SharedTextureMemoryTestBackend {
  public:
    static SharedTextureMemoryTestBackend* GetInstance() {
        static Backend b;
        return &b;
    }

    std::string Name() const override { return "AHardwareBuffer"; }

    std::vector<wgpu::FeatureName> RequiredFeatures(const wgpu::Adapter&) const override {
        return {wgpu::FeatureName::SharedTextureMemoryAHardwareBuffer,
                wgpu::FeatureName::SharedFenceVkSemaphoreSyncFD};
    }

    static std::string MakeLabel(const AHardwareBuffer_Desc& desc) {
        std::string label = std::to_string(desc.width) + "x" + std::to_string(desc.height);
        ;
        switch (desc.format) {
            case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
                label += " R8G8B8A8_UNORM";
                break;
            case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
                label += " R8G8B8X8_UNORM";
                break;
            case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT:
                label += " R16G16B16A16_FLOAT";
                break;
            case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM:
                label += " R10G10B10A2_UNORM";
                break;
            case AHARDWAREBUFFER_FORMAT_R8_UNORM:
                label += " R8_UNORM";
                break;
        }
        if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER) {
            label += " GPU_DATA_BUFFER";
        }
        if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER) {
            label += " GPU_FRAMEBUFFER";
        }
        if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) {
            label += " GPU_SAMPLED_IMAGE";
        }
        return label;
    }

    template <typename CreateFn>
    auto CreateSharedTextureMemoryHelper(uint32_t size,
                                         AHardwareBuffer_Format format,
                                         AHardwareBuffer_UsageFlags usage,
                                         CreateFn createFn) {
        AHardwareBuffer_Desc aHardwareBufferDesc = {
            .width = size,
            .height = size,
            .layers = 1,
            .format = format,
            .usage = usage,
        };
        AHardwareBuffer* aHardwareBuffer;
        EXPECT_EQ(AHardwareBuffer_allocate(&aHardwareBufferDesc, &aHardwareBuffer), 0);

        wgpu::SharedTextureMemoryAHardwareBufferDescriptor stmAHardwareBufferDesc;
        stmAHardwareBufferDesc.handle = aHardwareBuffer;

        std::string label = MakeLabel(aHardwareBufferDesc);
        wgpu::SharedTextureMemoryDescriptor desc;
        desc.label = label.c_str();
        desc.nextInChain = &stmAHardwareBufferDesc;

        auto ret = createFn(desc);
        AHardwareBuffer_release(aHardwareBuffer);

        return ret;
    }

    // Create one basic shared texture memory. It should support most operations.
    wgpu::SharedTextureMemory CreateSharedTextureMemory(const wgpu::Device& device) override {
        return CreateSharedTextureMemoryHelper(
            16, AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
            static_cast<AHardwareBuffer_UsageFlags>(AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER |
                                                    AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER |
                                                    AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE),
            [&](const wgpu::SharedTextureMemoryDescriptor& desc) {
                return device.ImportSharedTextureMemory(&desc);
            });
    }

    std::vector<std::vector<wgpu::SharedTextureMemory>> CreatePerDeviceSharedTextureMemories(
        const std::vector<wgpu::Device>& devices) override {
        std::vector<std::vector<wgpu::SharedTextureMemory>> memories;

        for (auto format : {
                 AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
                 // AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM,
                 // AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT,
                 // AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM,
                 // AHARDWAREBUFFER_FORMAT_R8_UNORM,
             }) {
            for (auto usage : {
                     // TODO(crbug.com/dawn/2262): Test with a reduced set of texture usages. All
                     // usages are currently included to test
                     // the expected usages in SharedTextureMemoryTests.TextureUsage.
                     static_cast<AHardwareBuffer_UsageFlags>(
                         AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER |
                         AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER |
                         AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE),
                 }) {
                for (uint32_t size : {4, 64}) {
                    std::vector<wgpu::SharedTextureMemory> perDeviceMemories;
                    for (auto& device : devices) {
                        perDeviceMemories.push_back(CreateSharedTextureMemoryHelper(
                            size, format, usage,
                            [&](const wgpu::SharedTextureMemoryDescriptor& desc) {
                                return device.ImportSharedTextureMemory(&desc);
                            }));
                    }
                    memories.push_back(std::move(perDeviceMemories));
                }
            }
        }
        return memories;
    }

    wgpu::SharedFence ImportFenceTo(const wgpu::Device& importingDevice,
                                    const wgpu::SharedFence& fence) override {
        wgpu::SharedFenceExportInfo exportInfo;
        fence.ExportInfo(&exportInfo);

        switch (exportInfo.type) {
            case wgpu::SharedFenceType::VkSemaphoreSyncFD: {
                wgpu::SharedFenceVkSemaphoreSyncFDExportInfo vkExportInfo;
                exportInfo.nextInChain = &vkExportInfo;
                fence.ExportInfo(&exportInfo);

                wgpu::SharedFenceVkSemaphoreSyncFDDescriptor vkDesc;
                vkDesc.handle = vkExportInfo.handle;

                wgpu::SharedFenceDescriptor fenceDesc;
                fenceDesc.nextInChain = &vkDesc;
                return importingDevice.ImportSharedFence(&fenceDesc);
            }
            default:
                DAWN_UNREACHABLE();
        }
    }

    struct BackendBeginStateVk : public BackendBeginState {
        wgpu::SharedTextureMemoryVkImageLayoutBeginState imageLayouts{};
    };

    struct BackendEndStateVk : public BackendEndState {
        wgpu::SharedTextureMemoryVkImageLayoutEndState imageLayouts{};
    };

    std::unique_ptr<BackendBeginState> ChainInitialBeginState(
        wgpu::SharedTextureMemoryBeginAccessDescriptor* beginDesc) override {
        auto state = std::make_unique<BackendBeginStateVk>();
        state->imageLayouts.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        state->imageLayouts.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        beginDesc->nextInChain = &state->imageLayouts;
        return state;
    }

    std::unique_ptr<BackendEndState> ChainEndState(
        wgpu::SharedTextureMemoryEndAccessState* endState) override {
        auto state = std::make_unique<BackendEndStateVk>();
        endState->nextInChain = &state->imageLayouts;
        return state;
    }

    std::unique_ptr<BackendBeginState> ChainBeginState(
        wgpu::SharedTextureMemoryBeginAccessDescriptor* beginDesc,
        const wgpu::SharedTextureMemoryEndAccessState& endState) override {
        DAWN_ASSERT(endState.nextInChain != nullptr);
        DAWN_ASSERT(endState.nextInChain->sType ==
                    wgpu::SType::SharedTextureMemoryVkImageLayoutEndState);
        auto* vkEndState =
            static_cast<wgpu::SharedTextureMemoryVkImageLayoutEndState*>(endState.nextInChain);

        auto state = std::make_unique<BackendBeginStateVk>();
        state->imageLayouts.oldLayout = vkEndState->oldLayout;
        state->imageLayouts.newLayout = vkEndState->newLayout;
        beginDesc->nextInChain = &state->imageLayouts;
        return state;
    }
};

DAWN_INSTANTIATE_PREFIXED_TEST_P(Vulkan,
                                 SharedTextureMemoryNoFeatureTests,
                                 {VulkanBackend()},
                                 {Backend::GetInstance()});

DAWN_INSTANTIATE_PREFIXED_TEST_P(Vulkan,
                                 SharedTextureMemoryTests,
                                 {VulkanBackend()},
                                 {Backend::GetInstance()});

}  // anonymous namespace
}  // namespace dawn
