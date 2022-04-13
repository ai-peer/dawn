// Copyright 2022 The Dawn Authors
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

#include "dawn/tests/DawnTest.h"

#include "dawn/tests/end2end/mocks/CachingInterfaceMock.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

#include <string_view>

namespace {
    using ::testing::NiceMock;
    using ::testing::SizeIs;
}  // namespace

class PipelineCachingTests : public DawnTest {
  protected:
    std::unique_ptr<dawn::platform::Platform> CreateTestPlatform() override {
        return std::make_unique<DawnCachingMockPlatform>(&mMockCache);
    }

    NiceMock<CachingInterfaceMock> mMockCache;
};

class SinglePipelineCachingTests : public PipelineCachingTests {};

TEST_P(SinglePipelineCachingTests, SameComputePipelineNoCache) {
    mMockCache.Disable();

    // Note that these tests need to use more than 1 device since the frontend cache on each device
    // will prevent even going out to the blob cache.
    constexpr std::string_view kComputeShader = R"(
        struct Data {
            data : u32
        }
        @binding(0) @group(0) var<storage, read_write> data : Data;

        @stage(compute) @workgroup_size(1) fn main() {
            data.data = 1u;
        }
    )";

    // First time should create and write out to the cache.
    {
        wgpu::Device device1 = wgpu::Device::Acquire(GetAdapter().CreateDevice());
        wgpu::ComputePipelineDescriptor desc;
        desc.compute.module = utils::CreateShaderModule(device1, kComputeShader.data());
        desc.compute.entryPoint = "main";
        EXPECT_CACHE_HIT(mMockCache, 0u, device.CreateComputePipeline(&desc));
    }
    EXPECT_THAT(mMockCache, SizeIs(1u));
}

DAWN_INSTANTIATE_TEST(SinglePipelineCachingTests, VulkanBackend());
