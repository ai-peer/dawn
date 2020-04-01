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

#include "dawn_native/D3D12Backend.h"
#include "dawn_native/d3d12/BufferD3D12.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/HeapD3D12.h"
#include "dawn_native/d3d12/ResidencyManagerD3D12.h"
#include "tests/DawnTest.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

#include <vector>

class D3D12ResidencyTests : public DawnTest {
  protected:
    void TestSetUp() override {
        DAWN_SKIP_TEST_IF(UsesWire());
        RestrictDawnBudgetCap(100000000);  // 100MB
    }

    void AllocateBuffers(uint32_t bufferSize,
                         uint32_t numberOfBuffers,
                         std::vector<wgpu::Buffer>* buffers) {
        for (uint64_t i = 0; i < numberOfBuffers; i++) {
            buffers->push_back(CreateBuffer(bufferSize, wgpu::BufferUsage::CopyDst));
        }
    }

    bool CheckIfBufferIsResident(wgpu::Buffer buffer) const {
        dawn_native::d3d12::Buffer* d3dBuffer =
            reinterpret_cast<dawn_native::d3d12::Buffer*>(buffer.Get());
        return d3dBuffer->CheckIsResidentForTesting();
    }

    bool IsUMA() const {
        return reinterpret_cast<dawn_native::d3d12::Device*>(device.Get())->GetDeviceInfo().isUMA;
    }

    wgpu::Buffer CreateBuffer(uint32_t bufferSize, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor descriptor;

        descriptor.size = bufferSize;
        descriptor.usage = usage;

        return device.CreateBuffer(&descriptor);
    }

    static void MapReadCallback(WGPUBufferMapAsyncStatus status,
                                const void* data,
                                uint64_t,
                                void* userdata) {
        ASSERT_EQ(WGPUBufferMapAsyncStatus_Success, status);
        ASSERT_NE(nullptr, data);

        static_cast<D3D12ResidencyTests*>(userdata)->mMappedReadData = data;
    }

    static void MapWriteCallback(WGPUBufferMapAsyncStatus status,
                                 void* data,
                                 uint64_t,
                                 void* userdata) {
        ASSERT_EQ(WGPUBufferMapAsyncStatus_Success, status);
        ASSERT_NE(nullptr, data);

        static_cast<D3D12ResidencyTests*>(userdata)->mMappedWriteData = data;
    }

    void RestrictDawnBudgetCap(uint64_t artificalBudgetCap) {
        dawn_native::d3d12::Device* d3dDevice =
            reinterpret_cast<dawn_native::d3d12::Device*>(device.Get());
        d3dDevice->GetResidencyManager()->RestrictBudgetForTesting(artificalBudgetCap);
    }

    void TouchBuffers(uint32_t bufferSize,
                      uint32_t beginIndex,
                      uint32_t numBuffers,
                      std::vector<wgpu::Buffer>& bufferSet) {
        // Initialize a source buffer on the GPU to serve as a source to quickly copy data to other
        // buffers.
        wgpu::Buffer sourceBuffer =
            CreateBuffer(bufferSize, wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst);

        std::vector<uint8_t> data(bufferSize);
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<uint8_t>(1);
        }

        wgpu::Buffer stagingBuffer = utils::CreateBufferFromData(
            device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(stagingBuffer, 0, sourceBuffer, 0, bufferSize);

        // Perform a copy on the range of textures to ensure the are moved to dedicated GPU memory.
        for (uint32_t i = beginIndex; i < beginIndex + numBuffers; i++) {
            encoder.CopyBufferToBuffer(sourceBuffer, 0, bufferSet[i], 0, bufferSize);
        }
        wgpu::CommandBuffer copy = encoder.Finish();
        queue.Submit(1, &copy);
    }

    void* mMappedWriteData = nullptr;
    const void* mMappedReadData = nullptr;
};

// Check that resources existing on suballocated heaps are made resident and evicted correctly.
TEST_P(D3D12ResidencyTests, OvercommitSmallResources) {
    // Use 1MB buffers. Internally, this causes a suballocated heap to be used.
    constexpr uint64_t kBufferSize = 1000000;

    // Allocate 50 buffers that are 1MB each. These will all fit into the 100MB budget, so they
    // should all be resident.
    std::vector<wgpu::Buffer> bufferSet1;
    AllocateBuffers(kBufferSize, 50, &bufferSet1);

    // Check that all the buffers allocated are resident.
    for (int i = 0; i < 50; i++) {
        ASSERT_TRUE(CheckIfBufferIsResident(bufferSet1[i]));
    }

    // Allocate 100 buffers that are 1MB each. This will cause everything currently resident to be
    // evicted.
    std::vector<wgpu::Buffer> bufferSet2;
    AllocateBuffers(kBufferSize, 100, &bufferSet2);

    // Check that everything in bufferSet1 is now evicted.
    for (int i = 0; i < 50; i++) {
        ASSERT_FALSE(CheckIfBufferIsResident(bufferSet1[i]));
    }

    // Touch one of the non-resident buffers. This should cause the buffer to become resident.
    constexpr uint32_t indexOfBufferInSet1 = 20;
    TouchBuffers(kBufferSize, indexOfBufferInSet1, 1, bufferSet1);
    // Check that this buffer is now resident.
    ASSERT_TRUE(CheckIfBufferIsResident(bufferSet1[indexOfBufferInSet1]));

    // Touch everything in bufferSet2 again to evict the buffer made resident in the previous
    // operation.
    TouchBuffers(kBufferSize, 0, 100, bufferSet2);
    // Check that indexOfBufferInSet1 was evicted.
    ASSERT_FALSE(CheckIfBufferIsResident(bufferSet1[indexOfBufferInSet1]));
}

// Check that resources existing on directly allocated heaps are made resident and evicted
// correctly.
TEST_P(D3D12ResidencyTests, OvercommitLargeResources) {
    // Use 5MB buffers. Internally, this causes directly allocated heaps to be used.
    constexpr uint64_t kBufferSize = 5000000;

    // Allocate 10 buffers that are 5MB each. These will all fit into the 100MB budget, so they
    // should all be resident.
    std::vector<wgpu::Buffer> bufferSet1;
    AllocateBuffers(kBufferSize, 10, &bufferSet1);

    // Check that all the buffers allocated are resident.
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(CheckIfBufferIsResident(bufferSet1[i]));
    }

    // Allocate 20 buffers that are 5MB each. This will cause everything currently resident to be
    // evicted.
    std::vector<wgpu::Buffer> bufferSet2;
    AllocateBuffers(kBufferSize, 20, &bufferSet2);

    // Check that everything in bufferSet1 is now evicted.
    for (int i = 0; i < 10; i++) {
        ASSERT_FALSE(CheckIfBufferIsResident(bufferSet1[i]));
    }

    // Touch one of the non-resident buffers. This should cause the buffer to become resident.
    constexpr uint32_t indexOfBufferInSet1 = 5;
    TouchBuffers(kBufferSize, indexOfBufferInSet1, 1, bufferSet1);
    ASSERT_TRUE(CheckIfBufferIsResident(bufferSet1[indexOfBufferInSet1]));

    // Touch everything in bufferSet2 again to evict the buffer made resident in the previous
    // operation.
    TouchBuffers(kBufferSize, 0, 20, bufferSet2);

    // Check that indexOfBufferInSet1 was evicted.
    ASSERT_FALSE(CheckIfBufferIsResident(bufferSet1[indexOfBufferInSet1]));
}

TEST_P(D3D12ResidencyTests, AsyncMappedBufferRead) {
    // Dawn currently only manages LOCAL_MEMORY. Mappable buffers exist in NON_LOCAL_MEMORY on
    // discrete devices.
    DAWN_SKIP_TEST_IF(!IsUMA())

    // Create a mappable buffer.
    wgpu::Buffer buffer = CreateBuffer(4, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);

    uint32_t data = 12345;
    buffer.SetSubData(0, sizeof(uint32_t), &data);

    // The mappable buffer should be resident.
    ASSERT_TRUE(CheckIfBufferIsResident(buffer));

    // Allocate and touch 20 buffers that are 5MB each. This will cause everything currently
    // resident to be evicted.
    constexpr uint64_t kSize = 5000000;  // 5MB
    std::vector<wgpu::Buffer> bufferSet;
    AllocateBuffers(kSize, 20, &bufferSet);
    TouchBuffers(kSize, 0, 20, bufferSet);

    // The mappable buffer should have been evicted.
    ASSERT_FALSE(CheckIfBufferIsResident(buffer));

    // Calling MapReadAsync should make the buffer resident.
    buffer.MapReadAsync(MapReadCallback, this);
    ASSERT_TRUE(CheckIfBufferIsResident(buffer));

    while (mMappedReadData == nullptr) {
        WaitABit();
    }

    // Touch 20 5MB buffers. This will cause everything currently resident to be evicted, however
    // the mapped buffer should be locked resident.
    TouchBuffers(kSize, 0, 20, bufferSet);

    ASSERT_TRUE(CheckIfBufferIsResident(buffer));

    // Unmap the buffer and touch 20 5MB buffers. this will cause the mappable buffer to be evicted.
    buffer.Unmap();
    TouchBuffers(kSize, 0, 20, bufferSet);

    ASSERT_FALSE(CheckIfBufferIsResident(buffer));
}

TEST_P(D3D12ResidencyTests, AsyncMappedBufferWrite) {
    // Dawn currently only manages LOCAL_MEMORY. Mappable buffers exist in NON_LOCAL_MEMORY on
    // discrete devices.
    DAWN_SKIP_TEST_IF(!IsUMA())

    // Create a mappable buffer.
    wgpu::Buffer buffer = CreateBuffer(4, wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc);

    // The mappable buffer should be resident.
    ASSERT_TRUE(CheckIfBufferIsResident(buffer));

    // Allocate and touch 20 buffers that are 5MB each. This will cause everything currently
    // resident to be evicted.
    constexpr uint64_t kSize = 5000000;  // 5MB
    std::vector<wgpu::Buffer> bufferSet;
    AllocateBuffers(kSize, 20, &bufferSet);
    TouchBuffers(kSize, 0, 20, bufferSet);

    // The mappable buffer should have been evicted.
    ASSERT_FALSE(CheckIfBufferIsResident(buffer));

    // Calling MapWriteAsync should make the buffer resident.
    buffer.MapWriteAsync(MapWriteCallback, this);
    ASSERT_TRUE(CheckIfBufferIsResident(buffer));

    while (mMappedWriteData == nullptr) {
        WaitABit();
    }

    // Touch 20 5MB buffers. This would usually cause everything currently resident to be evicted,
    // however the mapped buffer should be locked resident.
    TouchBuffers(kSize, 0, 20, bufferSet);
    ASSERT_TRUE(CheckIfBufferIsResident(buffer));

    // Unmap the buffer and touch 20 5MB buffers. this will cause the mappable buffer to be evicted.
    buffer.Unmap();

    TouchBuffers(kSize, 0, 20, bufferSet);
    ASSERT_FALSE(CheckIfBufferIsResident(buffer));
}

DAWN_INSTANTIATE_TEST(D3D12ResidencyTests,
                      D3D12Backend({"restrict_d3d12_residency_budget_for_testing"}));