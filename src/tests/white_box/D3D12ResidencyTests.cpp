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
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/ResidencyManagerD3D12.h"
#include "tests/DawnTest.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

class D3D12ResidencyTests : public DawnTest {
  protected:
    void AllocateBuffers(uint32_t bufferSize, uint64_t bytesToAllocate) {
        uint64_t numberOfResources = bytesToAllocate / bufferSize;

        for (uint64_t i = 0; i < numberOfResources; i++) {
            mBuffers.push_back(CreateBuffer(bufferSize, wgpu::BufferUsage::CopyDst));
        }
    }

    wgpu::Buffer CreateBuffer(uint32_t bufferSize, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor descriptor;

        descriptor.size = bufferSize;
        descriptor.usage = usage;

        return device.CreateBuffer(&descriptor);
    }

    uint64_t GetDawnBudget() const {
        dawn_native::d3d12::Device* d3dDevice =
            reinterpret_cast<dawn_native::d3d12::Device*>(device.Get());
        return d3dDevice->GetResidencyManager()->GetDawnBudgetForTesting();
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

    void TouchBuffers(uint32_t bufferSize, uint32_t beginIndex, uint32_t endIndex) {
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
        for (uint32_t i = beginIndex; i < endIndex; i++) {
            encoder.CopyBufferToBuffer(sourceBuffer, 0, mBuffers[i], 0, bufferSize);
        }
        wgpu::CommandBuffer copy = encoder.Finish();
        queue.Submit(1, &copy);
    }

    std::vector<wgpu::Buffer> mBuffers;

    void* mMappedWriteData = nullptr;
    const void* mMappedReadData = nullptr;
};

TEST_P(D3D12ResidencyTests, OvercommitSmallResources) {
    // Allocate 1.5x the available budget to ensure some buffers must be non-resident.
    // Use 1MB buffers, which will use sub-allocated resources internally.
    constexpr uint64_t kSize = 1048576;
    AllocateBuffers(kSize, GetDawnBudget() * 1.5);

    // Copy data to the first half of buffers. Since we allocated 1.5x Dawn's budget, about 75% of
    // these should have been paged out previously. Touching these will ensure all of them get
    // paged-in.
    TouchBuffers(kSize, 0, mBuffers.size() / 2);

    // Copy data to the second half of textures. About 25% of these should already be resident, and
    // the remainder must be paged back in after evicting the first half of textures.
    TouchBuffers(kSize, mBuffers.size() / 2, mBuffers.size());
}

TEST_P(D3D12ResidencyTests, OvercommitLargeResources) {
    // Allocate 1.5x the available budget to ensure some textures must be non-resident.
    // Use 2048 x 2048 images to make each texture 16MB, which must be directly allocated
    // internally.
    constexpr uint64_t kSize = 1048576;
    AllocateBuffers(kSize, GetDawnBudget() * 1.5);

    // Copy data to the first half of buffers. Since we allocated 1.5x Dawn's budget, about 75% of
    // these should have been paged out previously. Touching these will ensure all of them get
    // paged-in.
    TouchBuffers(kSize, 0, mBuffers.size() / 2);

    // Copy data to the second half of buffers. About 25% of these should already be resident, and
    // the remainder must be paged back in after evicting the first half of buffers.
    TouchBuffers(kSize, mBuffers.size() / 2, mBuffers.size());
}

TEST_P(D3D12ResidencyTests, AsyncMappedBufferRead) {
    // Create a mappable buffer.
    wgpu::Buffer buffer = CreateBuffer(4, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);

    uint32_t data = 12345;
    buffer.SetSubData(0, sizeof(uint32_t), &data);

    // Allocate 1.5x the available budget to ensure resources are being paged out. This should evict
    // the mappable buffer.
    constexpr uint64_t kSize = 1048576;
    AllocateBuffers(kSize, GetDawnBudget() * 1.5);

    // Calling MapReadAsync should make the buffer resident.
    buffer.MapReadAsync(MapReadCallback, this);

    while (mMappedReadData == nullptr) {
        WaitABit();
    }

    ASSERT_EQ(data, *reinterpret_cast<const uint32_t*>(mMappedReadData));

    buffer.Unmap();
}

TEST_P(D3D12ResidencyTests, AsyncMappedBufferWrite) {
    // Create a mappable buffer.
    wgpu::Buffer buffer = CreateBuffer(4, wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc);

    // Allocate 1.5x the available budget to ensure resources are being paged out. This should evict
    // the mappable buffer.
    constexpr uint64_t kSize = 1048576;
    AllocateBuffers(kSize, GetDawnBudget() * 1.5);

    // Calling MapWriteAsync should make the buffer resident.
    buffer.MapWriteAsync(MapWriteCallback, this);

    while (mMappedWriteData == nullptr) {
        WaitABit();
    }

    uint32_t data = 12345;
    memcpy(mMappedWriteData, &data, sizeof(uint32_t));
    buffer.Unmap();

    EXPECT_BUFFER_U32_EQ(data, buffer, 0);
}

TEST_P(D3D12ResidencyTests, AsyncMappedBufferLockedResidency) {
    // Create a mappable buffer.
    wgpu::Buffer buffer = CreateBuffer(4, wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc);

    // Map the buffer
    buffer.MapWriteAsync(MapWriteCallback, this);

    while (mMappedWriteData == nullptr) {
        WaitABit();
    }

    // Allocate 1.5x the available budget. This would normally evict the buffer, however Dawn should
    // not evict mapped buffers
    constexpr uint64_t kSize = 1048576;
    AllocateBuffers(kSize, GetDawnBudget() * 1.5);

    // Write to the mapped buffer.
    uint32_t data = 12345;
    memcpy(mMappedWriteData, &data, sizeof(uint32_t));

    buffer.Unmap();

    EXPECT_BUFFER_U32_EQ(data, buffer, 0);
}

DAWN_INSTANTIATE_TEST(D3D12ResidencyTests, D3D12Backend());