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

#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/PipelineCacheD3D12.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

using namespace dawn_native::d3d12;

#define EXPECT_PSO_CACHE_HIT(N, statement, cache)                    \
    do {                                                             \
        size_t before = cache->GetPipelineCacheHitCountForTesting(); \
        statement;                                                   \
        size_t after = cache->GetPipelineCacheHitCountForTesting();  \
        EXPECT_EQ(N, after - before);                                \
    } while (0)

#define EXPECT_NO_ERROR(statement)                  \
    do {                                            \
        dawn_native::MaybeError result = statement; \
        EXPECT_TRUE(result.IsSuccess());            \
    } while (0)

PipelineCache* GetPipelineCache(wgpu::Device device) {
    Device* d3dDevice = reinterpret_cast<Device*>(device.Get());
    return d3dDevice->GetPipelineCache();
}

class D3D12PipelineCachingTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
        DAWN_SKIP_TEST_IF(UsesWire());
    }
};

// Test creating a render pipeline with two shaders on the device then again but with a different
// device.
TEST_P(D3D12PipelineCachingTests, SameRenderPipeline) {
    size_t cacheSize = getPersistentCacheSize();

    const char* vs = R"(
                #version 450
                void main() {
                    gl_Position = vec4(0.0);
                })";

    const char* ps = R"(
                #version 450
                void main() {
                })";

    {
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{1, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

        wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);

        utils::ComboRenderPipelineDescriptor desc(device);
        desc.vertexStage.module =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, vs);

        desc.cFragmentStage.module =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, ps);

        desc.layout = pl;

        EXPECT_PSO_CACHE_HIT(0u, device.CreateRenderPipeline(&desc), GetPipelineCache(device));
    }

    EXPECT_NO_ERROR(GetPipelineCache(device)->storePipelineCache());

    // Both shaders and the PSO are persistently stored.
    EXPECT_EQ(getPersistentCacheSize(), cacheSize + 3);

    // Recreate the same pipeline from a different device.
    wgpu::Device device2 = GetAdapter().CreateDevice();
    {
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device2, {{1, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

        wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device2, &bgl);

        utils::ComboRenderPipelineDescriptor desc(device2);
        desc.vertexStage.module =
            utils::CreateShaderModule(device2, utils::SingleShaderStage::Vertex, vs);

        desc.cFragmentStage.module =
            utils::CreateShaderModule(device2, utils::SingleShaderStage::Fragment, ps);

        desc.layout = pl;

        EXPECT_PSO_CACHE_HIT(1u, device2.CreateRenderPipeline(&desc), GetPipelineCache(device2));
    }

    // Nothing new should be persistently stored.
    EXPECT_EQ(getPersistentCacheSize(), cacheSize + 3);
}

// Test creating a render pipeline with two shaders with two entry points on the device then again
// but with a different device.
TEST_P(D3D12PipelineCachingTests, SameRenderPipelineTwoEntryPoints) {
    size_t cacheSize = getPersistentCacheSize();

    const char* shader = R"(
        [[builtin position]] var<out> Position : vec4<f32>;
        fn vertex_main() -> void {
            Position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
            return;
        }
        entry_point vertex = vertex_main;

        [[location 0]] var<out> outColor : vec4<f32>;
        fn fragment_main() -> void {
          outColor = vec4<f32>(1.0, 0.0, 0.0, 1.0);
          return;
        }
        entry_point fragment = fragment_main;
    )";

    {
        wgpu::ShaderModule module = utils::CreateShaderModuleFromWGSL(device, shader);

        utils::ComboRenderPipelineDescriptor desc(device);
        desc.vertexStage.module = module;
        desc.vertexStage.entryPoint = "vertex_main";
        desc.cFragmentStage.module = module;
        desc.cFragmentStage.entryPoint = "fragment_main";

        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{1, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

        wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);
        desc.layout = pl;

        EXPECT_PSO_CACHE_HIT(0u, device.CreateRenderPipeline(&desc), GetPipelineCache(device));
    }

    EXPECT_NO_ERROR(GetPipelineCache(device)->storePipelineCache());

    // Ensure both shaders and the PSO were stored in the cache.
    EXPECT_EQ(getPersistentCacheSize(), cacheSize + 3);

    {
        wgpu::Device device2 = GetAdapter().CreateDevice();

        wgpu::ShaderModule module = utils::CreateShaderModuleFromWGSL(device2, shader);

        utils::ComboRenderPipelineDescriptor desc(device2);
        desc.vertexStage.module = module;
        desc.vertexStage.entryPoint = "vertex_main";
        desc.cFragmentStage.module = module;
        desc.cFragmentStage.entryPoint = "fragment_main";

        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device2, {{1, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

        wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device2, &bgl);
        desc.layout = pl;

        EXPECT_PSO_CACHE_HIT(1u, device2.CreateRenderPipeline(&desc), GetPipelineCache(device2));
    }

    // Nothing new should be persistently stored.
    EXPECT_EQ(getPersistentCacheSize(), cacheSize + 3);
}

DAWN_INSTANTIATE_TEST(D3D12PipelineCachingTests, D3D12Backend());
