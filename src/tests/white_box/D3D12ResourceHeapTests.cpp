// Copyright 2019 The Dawn Authors
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

#include "dawn_native/d3d12/BufferD3D12.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/TextureD3D12.h"

#include <list>

// Pooling tests are required to advance the GPU completed serial to reuse heaps.
// This requires Tick() to be called at-least |kFrameDepth| times. This constant
// should be updated if the internals of Tick() change.
constexpr uint32_t kFrameDepth = 3;

using namespace dawn_native::d3d12;

class D3D12ResourceHeapTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
        DAWN_SKIP_TEST_IF(UsesWire());
        mD3DDevice = reinterpret_cast<Device*>(device.Get());
    }

    std::vector<const char*> GetRequiredExtensions() override {
        mIsBCFormatSupported = SupportsExtensions({"texture_compression_bc"});
        if (!mIsBCFormatSupported) {
            return {};
        }

        return {"texture_compression_bc"};
    }

    bool IsBCFormatSupported() const {
        return mIsBCFormatSupported;
    }

    Device* mD3DDevice = nullptr;

  private:
    bool mIsBCFormatSupported = false;
};

// Verify that creating a small compressed textures will be 4KB aligned.
TEST_P(D3D12ResourceHeapTests, AlignSmallCompressedTexture) {
    DAWN_SKIP_TEST_IF(!IsBCFormatSupported());

    // TODO(http://crbug.com/dawn/282): Investigate GPU/driver rejections of small alignment.
    DAWN_SKIP_TEST_IF(IsIntel() || IsNvidia() || IsWARP());

    wgpu::TextureDescriptor descriptor;
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.size.width = 8;
    descriptor.size.height = 8;
    descriptor.size.depth = 1;
    descriptor.arrayLayerCount = 1;
    descriptor.sampleCount = 1;
    descriptor.format = wgpu::TextureFormat::BC1RGBAUnorm;
    descriptor.mipLevelCount = 1;
    descriptor.usage = wgpu::TextureUsage::Sampled;

    // Create a smaller one that allows use of the smaller alignment.
    wgpu::Texture texture = device.CreateTexture(&descriptor);
    Texture* d3dTexture = reinterpret_cast<Texture*>(texture.Get());

    EXPECT_EQ(d3dTexture->GetD3D12Resource()->GetDesc().Alignment,
              static_cast<uint64_t>(D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT));

    // Create a larger one (>64KB) that forbids use the smaller alignment.
    descriptor.size.width = 4096;
    descriptor.size.height = 4096;

    texture = device.CreateTexture(&descriptor);
    d3dTexture = reinterpret_cast<Texture*>(texture.Get());

    EXPECT_EQ(d3dTexture->GetD3D12Resource()->GetDesc().Alignment,
              static_cast<uint64_t>(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT));
}

// Verify a single resource heap can be reused.
TEST_P(D3D12ResourceHeapTests, ReuseHeap) {
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 64 * 1024;
    descriptor.usage = wgpu::BufferUsage::None;

    std::set<dawn_native::ResourceHeapBase*> heaps = {};
    std::vector<wgpu::Buffer> buffers = {};

    constexpr uint32_t kNumOfHeaps = 1;

    // Sub-allocate |kNumOfHeaps| worth of buffers.
    while (heaps.size() <= kNumOfHeaps) {
        wgpu::Buffer buffer = device.CreateBuffer(&descriptor);
        Buffer* d3dBuffer = reinterpret_cast<Buffer*>(buffer.Get());
        dawn_native::ResourceHeapBase* heap =
            d3dBuffer->GetAllocationForTesting().GetResourceHeap();
        heaps.insert(heap);
        buffers.push_back(buffer);
    }

    EXPECT_EQ(mD3DDevice->GetResourceHeapPoolSizeForTesting(), 0u);
    EXPECT_TRUE(kNumOfHeaps < buffers.size());
}

// Verify resource heaps will be recycled for multiple submits.
// Creates |kNumOfBuffers| twice using the same heap.
TEST_P(D3D12ResourceHeapTests, PoolHeapsMultipleSubmits) {
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 64 * 1024;
    descriptor.usage = wgpu::BufferUsage::None;

    std::set<dawn_native::ResourceHeapBase*> heaps = {};

    constexpr uint32_t kNumOfBuffers = 1000;

    // Sub-allocate |kNumOfBuffers|.
    for (uint32_t i = 0; i < kNumOfBuffers; i++) {
        wgpu::Buffer buffer = device.CreateBuffer(&descriptor);
        Buffer* d3dBuffer = reinterpret_cast<Buffer*>(buffer.Get());
        heaps.insert(d3dBuffer->GetAllocationForTesting().GetResourceHeap());
        mD3DDevice->Tick();
    }

    EXPECT_EQ(mD3DDevice->GetResourceHeapPoolSizeForTesting(), 0u);

    // Repeat again reusing the same heap.
    for (uint32_t i = 0; i < kNumOfBuffers; i++) {
        wgpu::Buffer buffer = device.CreateBuffer(&descriptor);
        Buffer* d3dBuffer = reinterpret_cast<Buffer*>(buffer.Get());
        dawn_native::ResourceHeapBase* heap =
            d3dBuffer->GetAllocationForTesting().GetResourceHeap();
        EXPECT_FALSE(std::find(heaps.begin(), heaps.end(), heap) == heaps.end());
        mD3DDevice->Tick();
    }

    EXPECT_EQ(mD3DDevice->GetResourceHeapPoolSizeForTesting(), 0u);
}

// Verify resource heaps do not recycle in a pending submit.
// Allocates |kNumOfHeaps| worth of buffers twice without using the same heaps.
TEST_P(D3D12ResourceHeapTests, PoolHeapsInPendingSubmit) {
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 64 * 1024;
    descriptor.usage = wgpu::BufferUsage::None;

    std::set<dawn_native::ResourceHeapBase*> heaps = {};

    // Count by heap (vs number of buffers) to ensure there are exactly |kNumOfHeaps| worth of
    // buffers. Otherwise, the heap may be reused if not full.
    constexpr uint32_t kNumOfHeaps = 5;

    // Sub-allocate |kNumOfHeaps| worth of buffers.
    while (heaps.size() < kNumOfHeaps) {
        wgpu::Buffer buffer = device.CreateBuffer(&descriptor);
        Buffer* d3dBuffer = reinterpret_cast<Buffer*>(buffer.Get());
        dawn_native::ResourceHeapBase* heap =
            d3dBuffer->GetAllocationForTesting().GetResourceHeap();
        heaps.insert(heap);
    }

    EXPECT_EQ(mD3DDevice->GetResourceHeapPoolSizeForTesting(), 0u);

    // Repeat again without reusing the same heaps.
    while (heaps.size() < kNumOfHeaps) {
        wgpu::Buffer buffer = device.CreateBuffer(&descriptor);
        Buffer* d3dBuffer = reinterpret_cast<Buffer*>(buffer.Get());
        dawn_native::ResourceHeapBase* heap =
            d3dBuffer->GetAllocationForTesting().GetResourceHeap();
        EXPECT_TRUE(std::find(heaps.begin(), heaps.end(), heap) == heaps.end());
    }

    EXPECT_EQ(mD3DDevice->GetResourceHeapPoolSizeForTesting(), 0u);
}

// Verify resource heaps do not recycle in a pending submit but do so
// once no longer pending.
TEST_P(D3D12ResourceHeapTests, PoolHeapsInPendingAndMultipleSubmits) {
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 64 * 1024;
    descriptor.usage = wgpu::BufferUsage::None;

    std::set<dawn_native::ResourceHeapBase*> heaps = {};

    // Count by heap (vs number of buffers) to ensure there are exactly |kNumOfHeaps| worth of
    // buffers. Otherwise, the heap may be reused if not full.
    constexpr uint32_t kNumOfHeaps = 5;

    // Sub-allocate |kNumOfHeaps| worth of buffers.
    uint32_t numOfBuffers = 0;
    while (heaps.size() < kNumOfHeaps) {
        wgpu::Buffer buffer = device.CreateBuffer(&descriptor);
        Buffer* d3dBuffer = reinterpret_cast<Buffer*>(buffer.Get());
        heaps.insert(d3dBuffer->GetAllocationForTesting().GetResourceHeap());
        numOfBuffers++;
    }

    EXPECT_EQ(mD3DDevice->GetResourceHeapPoolSizeForTesting(), 0u);

    // Ensure heaps can be recycled by advancing the GPU by at-least |kFrameDepth|.
    for (uint32_t i = 0; i < kFrameDepth; i++) {
        mD3DDevice->Tick();
    }

    EXPECT_EQ(mD3DDevice->GetResourceHeapPoolSizeForTesting(), kNumOfHeaps);

    // Repeat again reusing the same heaps.
    for (uint32_t i = 0; i < numOfBuffers; i++) {
        wgpu::Buffer buffer = device.CreateBuffer(&descriptor);
        Buffer* d3dBuffer = reinterpret_cast<Buffer*>(buffer.Get());
        dawn_native::ResourceHeapBase* heap =
            d3dBuffer->GetAllocationForTesting().GetResourceHeap();
        EXPECT_FALSE(std::find(heaps.begin(), heaps.end(), heap) == heaps.end());
    }

    EXPECT_EQ(mD3DDevice->GetResourceHeapPoolSizeForTesting(), 0u);
}

DAWN_INSTANTIATE_TEST(D3D12ResourceHeapTests,
                      D3D12Backend(),
                      D3D12Backend({}, {"use_d3d12_resource_heap_tier2"}));