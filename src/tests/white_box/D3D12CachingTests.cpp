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
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

using namespace dawn_native::d3d12;

#define EXPECT_CACHE_HIT(N, statement)              \
    do {                                            \
        size_t before = mPersistentCache.mHitCount; \
        statement;                                  \
        size_t after = mPersistentCache.mHitCount;  \
        EXPECT_EQ(N, after - before);               \
    } while (0)

// FakePersistentCache implements a in-memory persistent cache.
class FakePersistentCache : public dawn_platform::CachingInterface {
  public:
    FakePersistentCache() = default;
    ~FakePersistentCache() override {
        // Verify no blob was duplicated.
        for (auto& item : mCache) {
            EXPECT_EQ(mCache.count(item.first), 1u);
        }
    }

    // PersistentCache API
    void StoreData(void* device,
                   const void* key,
                   size_t keySize,
                   const void* value,
                   size_t valueSize) override {
        if (mIsDisabled)
            return;
        const std::string keyStr(reinterpret_cast<const char*>(key), keySize);

        const uint8_t* value_start = reinterpret_cast<const uint8_t*>(value);
        std::vector<uint8_t> entry_value(value_start, value_start + valueSize);

        ASSERT(mCache.insert({keyStr, std::move(entry_value)}).second);
    }

    size_t LoadData(void* device,
                    const void* key,
                    size_t keySize,
                    void* value,
                    size_t valueSize) override {
        const std::string keyStr(reinterpret_cast<const char*>(key), keySize);
        auto entry = mCache.find(keyStr);
        if (entry == mCache.end()) {
            return 0;
        }
        if (valueSize >= entry->second.size()) {
            memcpy(value, entry->second.data(), entry->second.size());
        }
        mHitCount++;
        return entry->second.size();
    }

    using Blob = std::vector<uint8_t>;
    using FakeCache = std::unordered_map<std::string, Blob>;

    FakeCache mCache;

    size_t mHitCount = 0;
    bool mIsDisabled = false;
};

// Test platform that only supports caching.
class DawnTestPlatform : public dawn_platform::Platform {
  public:
    DawnTestPlatform(dawn_platform::CachingInterface* cachingInterface)
        : mCachingInterface(cachingInterface) {
    }
    ~DawnTestPlatform() override = default;

    dawn_platform::CachingInterface* GetCachingInterface() override {
        return mCachingInterface;
    }

    dawn_platform::CachingInterface* mCachingInterface = nullptr;
};

class D3D12CachingTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
        DAWN_SKIP_TEST_IF(UsesWire());
    }

    dawn_platform::Platform* CreateTestPlatform() override {
        return new DawnTestPlatform(&mPersistentCache);
    }

    FakePersistentCache mPersistentCache;
};

// Test that duplicate WGSL compilation still works even when the cache is not enabled.
TEST_P(D3D12CachingTests, SameShaderNoCache) {
    mPersistentCache.mIsDisabled = true;

    wgpu::ShaderModule module = utils::CreateShaderModuleFromWGSL(device, R"(
        [[builtin(position)]] var<out> Position : vec4<f32>;

        [[stage(vertex)]]
        fn vertex_main() -> void {
            Position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
            return;
        }

        [[location(0)]] var<out> outColor : vec4<f32>;

        [[stage(fragment)]]
        fn fragment_main() -> void {
          outColor = vec4<f32>(1.0, 0.0, 0.0, 1.0);
          return;
        }
    )");

    // Store the WGSL shader into the cache.
    {
        utils::ComboRenderPipelineDescriptor desc(device);
        desc.vertexStage.module = module;
        desc.vertexStage.entryPoint = "vertex_main";
        desc.cFragmentStage.module = module;
        desc.cFragmentStage.entryPoint = "fragment_main";

        EXPECT_CACHE_HIT(0u, device.CreateRenderPipeline(&desc));
    }

    EXPECT_EQ(mPersistentCache.mCache.size(), 0u);

    // Load the same WGSL shader from the cache.
    {
        utils::ComboRenderPipelineDescriptor desc(device);
        desc.vertexStage.module = module;
        desc.vertexStage.entryPoint = "vertex_main";
        desc.cFragmentStage.module = module;
        desc.cFragmentStage.entryPoint = "fragment_main";

        EXPECT_CACHE_HIT(0u, device.CreateRenderPipeline(&desc));
    }

    EXPECT_EQ(mPersistentCache.mCache.size(), 0u);
}

// Test creating a pipeline from two entrypoints in multiple stages will cache the correct number
// of HLSL shaders. WGSL shader should result into 2x2 cached entries of HLSL (stage x
// entrypoints)
TEST_P(D3D12CachingTests, ReuseShaderWithMultipleEntryPointsPerStage) {
    wgpu::ShaderModule module = utils::CreateShaderModuleFromWGSL(device, R"(
        [[builtin(position)]] var<out> Position : vec4<f32>;

        [[stage(vertex)]]
        fn vertex_main() -> void {
            Position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
            return;
        }

        [[location(0)]] var<out> outColor : vec4<f32>;

        [[stage(fragment)]]
        fn fragment_main() -> void {
          outColor = vec4<f32>(1.0, 0.0, 0.0, 1.0);
          return;
        }
    )");

    // Store the WGSL shader into the cache.
    {
        utils::ComboRenderPipelineDescriptor desc(device);
        desc.vertexStage.module = module;
        desc.vertexStage.entryPoint = "vertex_main";
        desc.cFragmentStage.module = module;
        desc.cFragmentStage.entryPoint = "fragment_main";

        EXPECT_CACHE_HIT(0u, device.CreateRenderPipeline(&desc));
    }

    EXPECT_EQ(mPersistentCache.mCache.size(), 2u);

    // Load the same WGSL shader from the cache.
    {
        utils::ComboRenderPipelineDescriptor desc(device);
        desc.vertexStage.module = module;
        desc.vertexStage.entryPoint = "vertex_main";
        desc.cFragmentStage.module = module;
        desc.cFragmentStage.entryPoint = "fragment_main";

        EXPECT_CACHE_HIT(4u, device.CreateRenderPipeline(&desc));
    }

    EXPECT_EQ(mPersistentCache.mCache.size(), 2u);

    // Modify the WGSL shader functions and make sure it doesn't hit.
    wgpu::ShaderModule newModule = utils::CreateShaderModuleFromWGSL(device, R"(
      [[builtin(position)]] var<out> Position : vec4<f32>;

      [[stage(vertex)]]
      fn vertex_main() -> void {
          Position = vec4<f32>(1.0, 1.0, 1.0, 1.0);
          return;
      }

      [[location(0)]] var<out> outColor : vec4<f32>;

      [[stage(fragment)]]
      fn fragment_main() -> void {
        outColor = vec4<f32>(1.0, 1.0, 1.0, 1.0);
        return;
      }
  )");

    {
        utils::ComboRenderPipelineDescriptor desc(device);
        desc.vertexStage.module = newModule;
        desc.vertexStage.entryPoint = "vertex_main";
        desc.cFragmentStage.module = newModule;
        desc.cFragmentStage.entryPoint = "fragment_main";
        EXPECT_CACHE_HIT(0u, device.CreateRenderPipeline(&desc));
    }

    EXPECT_EQ(mPersistentCache.mCache.size(), 4u);
}

// Test creating a WGSL shader with two entrypoints in the same stage will cache the correct number
// of HLSL shaders. WGSL shader should result into 2x1 cached entries of HLSL (stage x entrypoints)
TEST_P(D3D12CachingTests, ReuseShaderWithMultipleEntryPoints) {
    wgpu::ShaderModule module = utils::CreateShaderModuleFromWGSL(device, R"(
        [[block]] struct Data {
            [[offset(0)]] data : u32;
        };
        [[binding(0), set(0)]] var<storage_buffer> data : Data;

        [[stage(compute)]]
        fn write1() -> void {
            data.data = 1u;
            return;
        }

        [[stage(compute)]]
        fn write42() -> void {
            data.data = 42u;
            return;
        }
    )");

    // Store the WGSL shader into the cache.
    {
        wgpu::ComputePipelineDescriptor desc;
        desc.computeStage.module = module;
        desc.computeStage.entryPoint = "write1";
        EXPECT_CACHE_HIT(0u, device.CreateComputePipeline(&desc));

        desc.computeStage.module = module;
        desc.computeStage.entryPoint = "write42";
        EXPECT_CACHE_HIT(0u, device.CreateComputePipeline(&desc));
    }

    EXPECT_EQ(mPersistentCache.mCache.size(), 2u);

    // Load the same WGSL shader from the cache.
    {
        wgpu::ComputePipelineDescriptor desc;
        desc.computeStage.module = module;
        desc.computeStage.entryPoint = "write1";
        EXPECT_CACHE_HIT(2u, device.CreateComputePipeline(&desc));

        desc.computeStage.module = module;
        desc.computeStage.entryPoint = "write42";
        EXPECT_CACHE_HIT(2u, device.CreateComputePipeline(&desc));
    }

    EXPECT_EQ(mPersistentCache.mCache.size(), 2u);
}

DAWN_INSTANTIATE_TEST(D3D12CachingTests, D3D12Backend());