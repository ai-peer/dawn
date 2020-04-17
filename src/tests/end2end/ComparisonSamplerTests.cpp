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

#include "common/Assert.h"
#include "common/Constants.h"
#include "tests/DawnTest.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

class ComparisonSamplerTest : public DawnTest {
};

TEST_P(ComparisonSamplerTest, CompareFunctions) {
    wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(device,
        {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture}});

    wgpu::TextureDescriptor textureDesc = {
        .usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::Sampled,
        .size = {1, 1, 1},
        .format = wgpu::TextureFormat::Depth32Float, // Passes with RGBA8Unorm
    };

    wgpu::Texture texture = device.CreateTexture(&textureDesc);

    utils::MakeBindGroup(device, bgl, {{0, texture.CreateView()}});

    // Crashes on D3D12 with device lost in CommandAllocatorManager::Tick
    queue.Submit(0, nullptr);
}

DAWN_INSTANTIATE_TEST(ComparisonSamplerTest,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      VulkanBackend());
