// Copyright 2021 The Dawn Authors
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

#include "utils/WGPUHelpers.h"

class VulkanMemoryAllocationTests : public DawnTest {
  public:
    void SetUp() override {
        DawnTest::SetUp();
        DAWN_TEST_UNSUPPORTED_IF(UsesWire());
    }
};

TEST_P(VulkanMemoryAllocationTests, AllocateTextureThenBuffer) {
    wgpu::TextureDescriptor texDesc;
    texDesc.usage = wgpu::TextureUsage::Sampled | wgpu::TextureUsage::CopySrc;
    texDesc.format = wgpu::TextureFormat::RGBA8Unorm;
    texDesc.size = {1, 1};
    wgpu::Texture tex = device.CreateTexture(&texDesc);

    utils::ComboRenderPassDescriptor renderPass({tex.CreateView()});
    renderPass.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
    renderPass.cColorAttachments[0].clearColor = {0, 255, 0, 255};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    wgpu::BufferDescriptor bufDesc;
    bufDesc.usage = wgpu::BufferUsage::Storage;
    bufDesc.size = 4;
    wgpu::Buffer buf = device.CreateBuffer(&bufDesc);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8::kGreen, tex, 0, 0);
}


DAWN_INSTANTIATE_TEST(VulkanMemoryAllocationTests, VulkanBackend());
