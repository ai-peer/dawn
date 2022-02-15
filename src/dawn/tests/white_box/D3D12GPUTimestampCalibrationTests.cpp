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

#include "dawn/tests/DawnTest.h"

#include "dawn/native/Buffer.h"
#include "dawn/native/CommandEncoder.h"
#include "dawn/native/d3d12/DeviceD3D12.h"
#include "dawn/utils/WGPUHelpers.h"

namespace {
    class ExpectTimestampsBetween : public detail::Expectation {
      public:
        ~ExpectTimestampsBetween() override = default;

        ExpectTimestampsBetween(uint64_t value0, uint64_t value1) {
            mValue0 = value0;
            mValue1 = value1;
        }

        // Expect the actual results are between mValue0 and mValue1.
        testing::AssertionResult Check(const void* data, size_t size) override {
            const uint64_t* actual = static_cast<const uint64_t*>(data);
            for (size_t i = 0; i < size / sizeof(uint64_t); ++i) {
                if (actual[i] <= mValue0 || actual[i] >= mValue1) {
                    return testing::AssertionFailure()
                           << "Expected data[" << i << "] " << actual[i] << " is not between "
                           << mValue0 << " and " << mValue1 << ". " << std::endl;
                }
            }

            return testing::AssertionSuccess();
        }

      private:
        uint64_t mValue0;
        uint64_t mValue1;
    };

}  // anonymous namespace

using namespace dawn::native::d3d12;

class D3D12GPUTimestampCalibrationTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        // Skip all tests if timestamp feature is not supported
        DAWN_TEST_UNSUPPORTED_IF(!SupportsFeatures({wgpu::FeatureName::TimestampQuery}));
    }

    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        std::vector<wgpu::FeatureName> requiredFeatures = {};
        if (SupportsFeatures({wgpu::FeatureName::TimestampQuery})) {
            requiredFeatures.push_back(wgpu::FeatureName::TimestampQuery);
        }
        return requiredFeatures;
    }

    wgpu::Buffer CreateResolveBuffer(uint64_t size) {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = size;
        descriptor.usage = wgpu::BufferUsage::QueryResolve | wgpu::BufferUsage::CopySrc |
                           wgpu::BufferUsage::CopyDst;
        return device.CreateBuffer(&descriptor);
    }
};

// Track that the timestamps got by timestamp query cannot be calibrated with the timestamps in
// GetClockCalibration(). The timestamps got by timestamp query are coverted by the compute shader
// with precision loss (3e10-5). Although the loss of precision is not large, the timestamps are
// 64-bit unsigned integer, the error of the converted timestamps will be in milliseconds or
// seconds, which lead to they cannot be used in the calibration.
TEST_P(D3D12GPUTimestampCalibrationTests, TimestampPrecision) {
    constexpr uint32_t kQueryCount = 2;

    wgpu::QuerySetDescriptor descriptor;
    descriptor.count = kQueryCount;
    descriptor.type = wgpu::QueryType::Timestamp;
    wgpu::QuerySet querySet = device.CreateQuerySet(&descriptor);

    wgpu::Buffer destination = CreateResolveBuffer(kQueryCount * sizeof(uint64_t));

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.WriteTimestamp(querySet, 0);
    encoder.WriteTimestamp(querySet, 1);
    encoder.ResolveQuerySet(querySet, 0, kQueryCount, destination, 0);
    wgpu::CommandBuffer commands = encoder.Finish();

    Device* d3DDevice = reinterpret_cast<Device*>(device.Get());
    uint64_t gpuTimestamp0, gpuTimestamp1;
    uint64_t cpuTimestamp0, cpuTimestamp1;
    d3DDevice->GetCommandQueue()->GetClockCalibration(&gpuTimestamp0, &cpuTimestamp0);
    queue.Submit(1, &commands);
    WaitForAllOperations();
    d3DDevice->GetCommandQueue()->GetClockCalibration(&gpuTimestamp1, &cpuTimestamp1);

    uint64_t gpuFrequency;
    d3DDevice->GetCommandQueue()->GetTimestampFrequency(&gpuFrequency);
    float period = static_cast<float>(1000000000) / gpuFrequency;

    EXPECT_BUFFER(destination, 0, kQueryCount * sizeof(uint64_t),
                  new ExpectTimestampsBetween(
                      static_cast<uint64_t>(static_cast<double>(gpuTimestamp0 * period)),
                      static_cast<uint64_t>(static_cast<double>(gpuTimestamp1 * period))));
}

DAWN_INSTANTIATE_TEST(D3D12GPUTimestampCalibrationTests, D3D12Backend());
