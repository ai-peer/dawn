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

#include <gtest/gtest.h>
#include <vector>
#include "dawn/tests/DawnTest.h"
#include "dawn/utils/WGPUHelpers.h"

using ::testing::HasSubstr;

namespace dawn {

class SharedBufferMemoryTests : public DawnTest {
    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        return {wgpu::FeatureName::SharedBufferMemoryD3D12Resource};
    }
};

// Test that it is an error to import shared buffer memory when the device is destroyed
TEST_P(SharedBufferMemoryTests, ImportSharedBufferMemoryNoChain) {
    DAWN_TEST_UNSUPPORTED_IF(UsesWire());
    wgpu::SharedBufferMemoryDescriptor desc;
    ASSERT_DEVICE_ERROR_MSG(
        wgpu::SharedBufferMemory memory = device.ImportSharedBufferMemory(&desc),
        HasSubstr("chain"));
}

// Test that it is an error importing shared buffer memory when the device is destroyed
TEST_P(SharedBufferMemoryTests, ImportSharedBufferMemoryDeviceDestroy) {
    DAWN_TEST_UNSUPPORTED_IF(UsesWire());
    device.Destroy();

    wgpu::SharedBufferMemoryDescriptor desc;
    ASSERT_DEVICE_ERROR_MSG(
        wgpu::SharedBufferMemory memory = device.ImportSharedBufferMemory(&desc),
        HasSubstr("lost"));
}

TEST_P(SharedBufferMemoryTests, GetPropertiesErrorMemory) {
    DAWN_TEST_UNSUPPORTED_IF(UsesWire());
    wgpu::SharedBufferMemoryDescriptor desc;
    ASSERT_DEVICE_ERROR(wgpu::SharedBufferMemory memory = device.ImportSharedBufferMemory(&desc));

    wgpu::SharedBufferMemoryProperties properties;
    memory.GetProperties(&properties);

    EXPECT_EQ(properties.usage, wgpu::BufferUsage::None);
    EXPECT_EQ(properties.size, 0u);
}

DAWN_INSTANTIATE_TEST(SharedBufferMemoryTests, D3D12Backend());

}  // namespace dawn
