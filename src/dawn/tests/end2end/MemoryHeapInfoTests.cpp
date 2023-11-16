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

#include <vector>

#include "dawn/tests/DawnTest.h"

namespace dawn {
namespace {

class MemoryHeapInfoNoFeatureTest : public DawnTest {};

class MemoryHeapInfoTest : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
        DAWN_TEST_UNSUPPORTED_IF(!device.HasFeature(wgpu::FeatureName::MemoryHeapInfo));
    }

    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        std::vector<wgpu::FeatureName> requiredFeatures = {};
        if (SupportsFeatures({wgpu::FeatureName::MemoryHeapInfo})) {
            requiredFeatures.push_back(wgpu::FeatureName::MemoryHeapInfo);
        }
        return requiredFeatures;
    }
};

// Test it is an error to query the heap info if the feature is not enabled.
TEST_P(MemoryHeapInfoNoFeatureTest, IsError) {
    ASSERT_DEVICE_ERROR(device.QueryMemoryHeapInfo(nullptr));

    wgpu::MemoryHeapInfo info;
    ASSERT_DEVICE_ERROR(device.QueryMemoryHeapInfo(&info));
}

// Test it is invalid to query the memory heaps after device destroy.
TEST_P(MemoryHeapInfoTest, QueryAfterDestroy) {
    device.Destroy();
    ASSERT_DEVICE_ERROR(device.QueryMemoryHeapInfo(nullptr));

    wgpu::MemoryHeapInfo info;
    ASSERT_DEVICE_ERROR(device.QueryMemoryHeapInfo(&info));
}

// Test that it is possible to query the memory, and it is populated with valid enums.
TEST_P(MemoryHeapInfoTest, QueryMemory) {
    size_t count = device.QueryMemoryHeapInfo(nullptr);

    std::vector<wgpu::MemoryHeapInfo> info(count);
    EXPECT_EQ(device.QueryMemoryHeapInfo(info.data()), count);

    for (const wgpu::MemoryHeapInfo& h : info) {
        EXPECT_GT(h.recommendedMaxSize, 0ull);

        constexpr wgpu::HeapProperty kValidProps =
            wgpu::HeapProperty::DeviceLocal | wgpu::HeapProperty::HostVisible |
            wgpu::HeapProperty::HostCoherent | wgpu::HeapProperty::HostCached;

        EXPECT_EQ(h.heapProperties & ~kValidProps, 0u);
    }
}

DAWN_INSTANTIATE_TEST(MemoryHeapInfoNoFeatureTest,
                      D3D11Backend(),
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());

DAWN_INSTANTIATE_TEST(MemoryHeapInfoTest,
                      D3D11Backend(),
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());

}  // anonymous namespace
}  // namespace dawn
