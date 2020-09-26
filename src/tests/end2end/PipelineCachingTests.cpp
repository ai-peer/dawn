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

#include "common/LinkedList.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

#include <vector>

wgpu::RenderPipeline CreateRenderPipeline(wgpu::Device device) {
    wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

    wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);

    utils::ComboRenderPipelineDescriptor desc(device);
    desc.vertexStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
                #version 450
                void main() {
                    gl_Position = vec4(0.0);
                })");

    desc.cFragmentStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
                #version 450
                void main() {
                })");

    desc.layout = pl;
    return device.CreateRenderPipeline(&desc);
}

class PipelineCachingTests : public DawnTest {
  public:
    using Blob = std::vector<uint8_t>;
    using FakeCache = std::unordered_map<std::string, Blob>;

    static void StorePersistentCache(const void* key,
                                     size_t keySize,
                                     const void* value,
                                     size_t valueSize,
                                     void* cacheData) {
        FakeCache* cache = reinterpret_cast<FakeCache*>(cacheData);
        const std::string keyStr(reinterpret_cast<const char*>(key), keySize);

        const uint8_t* value_start = reinterpret_cast<const uint8_t*>(value);
        std::vector<uint8_t> entry_value(value_start, value_start + valueSize);

        cache->insert({keyStr, std::move(entry_value)});
    }

    static size_t LoadPersistentCache(const void* key,
                                      size_t keySize,
                                      void* value,
                                      size_t valueSize,
                                      void* cacheData) {
        FakeCache* cache = reinterpret_cast<FakeCache*>(cacheData);
        std::string keyStr(reinterpret_cast<const char*>(key), keySize);
        size_t cache_size = cache->size();
        ASSERT(cache_size >= 0);
        auto entry = cache->find(keyStr);
        if (entry == cache->end()) {
            return 0;
        }
        if (valueSize >= entry->second.size()) {
            memcpy(value, entry->second.data(), entry->second.size());
        }
        return entry->second.size();
    }

    void SetUp() override {
        DawnTest::SetUp();
        if (UsesWire()) {
            return;
        }

        setPersistentCacheFuncs(PipelineCachingTests::StorePersistentCache,
                                PipelineCachingTests::LoadPersistentCache,
                                reinterpret_cast<void*>(&mFakePersistentCache));
    }

  protected:
    FakeCache mFakePersistentCache;
};

// Test creating the same pipeline on different devices.
TEST_P(PipelineCachingTests, RenderPipelineReload) {
    // TODO: Get this working
    DAWN_SKIP_TEST_IF(IsVulkan());

    wgpu::RenderPipeline pl1 = CreateRenderPipeline(device);

    EXPECT_EQ(mFakePersistentCache.size(), 2u);

    // Recreate the pipeline from the persistent cache.
    wgpu::Device device2 = GetAdapter().CreateDevice();
    wgpu::RenderPipeline pl2 = CreateRenderPipeline(device2);

    EXPECT_EQ(mFakePersistentCache.size(), 2u);
}

// TODO: Pipeline caching is only supported on Vulkan and D3D12.
DAWN_INSTANTIATE_TEST(PipelineCachingTests, D3D12Backend(), VulkanBackend());
