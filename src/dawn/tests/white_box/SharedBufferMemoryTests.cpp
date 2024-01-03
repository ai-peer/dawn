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
#include <d3d12.h>
#include <utility>
#include <vector>
#include "dawn/native/D3D12Backend.h"
#include "dawn/native/d3d12/DeviceD3D12.h"
#include "dawn/tests/DawnTest.h"
#include "dawn/tests/MockCallback.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {
constexpr uint32_t kBufferWidth = 32;
constexpr uint32_t kBufferData = 0xFFFFFFFF;
class D3D12SharedBufferMemoryTests : public DawnTest {
    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        return {wgpu::FeatureName::SharedBufferMemoryD3D12Resource,
                wgpu::FeatureName::SharedFenceDXGISharedHandle};
    }

    void SetUp() override {
        DawnTest::SetUp();
        DAWN_TEST_UNSUPPORTED_IF(UsesWire());

        // Create a D3D12 device and make a buffer for sharing.
        ComPtr<IDXGIAdapter> dxgiAdapter = native::d3d::GetDXGIAdapter(device.GetAdapter().Get());
        DXGI_ADAPTER_DESC adapterDesc;
        dxgiAdapter->GetDesc(&adapterDesc);

        ComPtr<IDXGIFactory4> dxgiFactory;
        CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory));
        dxgiAdapter = nullptr;
        dxgiFactory->EnumAdapterByLuid(adapterDesc.AdapterLuid, IID_PPV_ARGS(&dxgiAdapter));

        D3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                          &mExternalDevice);

        // Create a buffer for data upload
        D3D12_HEAP_PROPERTIES heapProperties = {D3D12_HEAP_TYPE_UPLOAD,
                                                D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                                D3D12_MEMORY_POOL_UNKNOWN, 0, 0};

        D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;

        D3D12_RESOURCE_DESC uploadDescriptor;
        uploadDescriptor.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDescriptor.Alignment = 0;
        uploadDescriptor.Width = kBufferWidth;
        uploadDescriptor.Height = 1;
        uploadDescriptor.DepthOrArraySize = 1;
        uploadDescriptor.MipLevels = 1;
        uploadDescriptor.Format = DXGI_FORMAT_UNKNOWN;
        uploadDescriptor.SampleDesc.Count = 1;
        uploadDescriptor.SampleDesc.Quality = 0;
        uploadDescriptor.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        uploadDescriptor.Flags = D3D12_RESOURCE_FLAG_NONE;

        mExternalDevice->CreateCommittedResource(&heapProperties, heapFlags, &uploadDescriptor,
                                                 D3D12_RESOURCE_STATE_COMMON, {},
                                                 IID_PPV_ARGS(&mUploadBuffer));

        // Create a buffer for readback
        heapProperties = {D3D12_HEAP_TYPE_READBACK, D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                          D3D12_MEMORY_POOL_UNKNOWN, 0, 0};

        uploadDescriptor.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDescriptor.Alignment = 0;
        uploadDescriptor.Width = kBufferWidth;
        uploadDescriptor.Height = 1;
        uploadDescriptor.DepthOrArraySize = 1;
        uploadDescriptor.MipLevels = 1;
        uploadDescriptor.Format = DXGI_FORMAT_UNKNOWN;
        uploadDescriptor.SampleDesc.Count = 1;
        uploadDescriptor.SampleDesc.Quality = 0;
        uploadDescriptor.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        uploadDescriptor.Flags = D3D12_RESOURCE_FLAG_NONE;

        mExternalDevice->CreateCommittedResource(&heapProperties, heapFlags, &uploadDescriptor,
                                                 D3D12_RESOURCE_STATE_COMMON, {},
                                                 IID_PPV_ARGS(&mReadbackBuffer));

        // Create a buffer for sharing
        heapProperties = {D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                          D3D12_MEMORY_POOL_UNKNOWN, 0, 0};

        D3D12_RESOURCE_DESC resourceDescriptor;
        resourceDescriptor.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDescriptor.Alignment = 0;
        resourceDescriptor.Width = kBufferWidth;
        resourceDescriptor.Height = 1;
        resourceDescriptor.DepthOrArraySize = 1;
        resourceDescriptor.MipLevels = 1;
        resourceDescriptor.Format = DXGI_FORMAT_UNKNOWN;
        resourceDescriptor.SampleDesc.Count = 1;
        resourceDescriptor.SampleDesc.Quality = 0;
        resourceDescriptor.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDescriptor.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        mExternalDevice->CreateCommittedResource(&heapProperties, heapFlags, &resourceDescriptor,
                                                 D3D12_RESOURCE_STATE_COMMON, {},
                                                 IID_PPV_ARGS(&mExternalBuffer));
    }

    // void

  public:
    void WriteBufferDataExternal(uint32_t data) {
        // Set data to upload buffer on CPU
        void* mappedBufferBegin;
        D3D12_RANGE range;
        range.Begin = 0;
        range.End = 8;
        mUploadBuffer->Map(0, &range, &mappedBufferBegin);
        ((uint32_t*)mappedBufferBegin)[0] = data;
        mUploadBuffer->Unmap(0, &range);

        // Copy upload buffer data to shareable buffer
        ComPtr<ID3D12GraphicsCommandList> commandList;

        ComPtr<ID3D12CommandAllocator> commandAllocator;
        mExternalDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                IID_PPV_ARGS(&commandAllocator));
        mExternalDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           commandAllocator.Get(), nullptr,
                                           IID_PPV_ARGS(&commandList));

        ID3D12CommandList* commandLists[] = {commandList.Get()};
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        commandList->CopyResource(mExternalBuffer.Get(), mUploadBuffer.Get());
        commandList->Close();
        ComPtr<ID3D12CommandQueue> commandQueue;
        mExternalDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
        commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

        HANDLE m_fenceEvent = 0;
        ComPtr<ID3D12Fence> mFence;
        mExternalDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));
        UINT64 fence = 0;
        commandQueue->Signal(mFence.Get(), 1);
        fence++;

        // Wait until the previous operation is finished.
        if (mFence->GetCompletedValue() == 0) {
            mFence->SetEventOnCompletion(fence, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }

    uint32_t ReadBufferDataExternal() {
        // Copy shared buffer data to the mappable upload buffer
        ComPtr<ID3D12GraphicsCommandList> commandList;

        ComPtr<ID3D12CommandAllocator> commandAllocator;
        mExternalDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                IID_PPV_ARGS(&commandAllocator));
        mExternalDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           commandAllocator.Get(), nullptr,
                                           IID_PPV_ARGS(&commandList));

        ID3D12CommandList* commandLists[] = {commandList.Get()};
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        commandList->CopyResource(mUploadBuffer.Get(), mExternalBuffer.Get());
        commandList->Close();
        ComPtr<ID3D12CommandQueue> commandQueue;
        mExternalDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
        commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

        HANDLE m_fenceEvent = 0;
        ComPtr<ID3D12Fence> mFence;
        mExternalDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));
        UINT64 fence = 0;
        commandQueue->Signal(mFence.Get(), 1);
        fence++;

        // Wait until the previous operation is finished.
        if (mFence->GetCompletedValue() == 0) {
            mFence->SetEventOnCompletion(fence, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }

        // Set data to upload buffer on CPU
        void* mappedBufferBegin;
        D3D12_RANGE range;
        range.Begin = 0;
        range.End = 8;
        mUploadBuffer->Map(0, &range, &mappedBufferBegin);
        uint32_t readBackData = ((uint32_t*)mappedBufferBegin)[0];
        mUploadBuffer->Unmap(0, &range);
        return readBackData;
    }

    void CreateSharedBufferMemory() {
        wgpu::SharedBufferMemoryDescriptor desc;
        native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
        sharedD3d12ResourceDesc.resource = mExternalBuffer.Get();
        desc.nextInChain = &sharedD3d12ResourceDesc;
        wgpu::SharedBufferMemory sharedBuffer = device.ImportSharedBufferMemory(&desc);
        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.size = kBufferWidth;
        bufferDesc.usage =
            wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::Storage;
        mDawnBuffer = sharedBuffer.CreateBuffer(&bufferDesc);
    }

    ComPtr<ID3D12Resource> mUploadBuffer;
    ComPtr<ID3D12Resource> mReadbackBuffer;
    ComPtr<ID3D12Resource> mExternalBuffer;
    ComPtr<ID3D12Device> mExternalDevice;
    wgpu::Buffer mDawnBuffer;
};

TEST_P(D3D12SharedBufferMemoryTests, CreateSharedBufferMemory) {
    CreateSharedBufferMemory();
}

TEST_P(D3D12SharedBufferMemoryTests, ReadSharedBufferMemory) {
    // Set buffer data outside of Dawn.
    WriteBufferDataExternal(kBufferData);

    // Import the D3D12 buffer into Dawn and create a wgpu::Buffer from it.
    wgpu::SharedBufferMemoryDescriptor desc;
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = mUploadBuffer.Get();
    desc.nextInChain = &sharedD3d12ResourceDesc;
    wgpu::SharedBufferMemory sharedBuffer = device.ImportSharedBufferMemory(&desc);
    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = kBufferWidth;
    bufferDesc.usage =
        wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::Storage;
    wgpu::Buffer buffer = sharedBuffer.CreateBuffer(&bufferDesc);

    // Read shared memory data from within Dawn
    wgpu::SharedBufferMemoryBeginAccessDescriptor beginAccessDesc;
    beginAccessDesc.fenceCount = 0;
    sharedBuffer.BeginAccess(buffer, &beginAccessDesc);
    wgpu::SharedBufferMemoryEndAccessState endAccessState = {};
    EXPECT_BUFFER_U32_EQ(kBufferData, buffer, 0);
    sharedBuffer.EndAccess(buffer, &endAccessState);
}

/*TEST_P(D3D12SharedBufferMemoryTests, WriteSharedBufferMemory) {
    // Import the D3D12 buffer into Dawn and create a wgpu::Buffer from it.
    wgpu::SharedBufferMemoryDescriptor desc;
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = mExternalBuffer.Get();
    desc.nextInChain = &sharedD3d12ResourceDesc;
    wgpu::SharedBufferMemory sharedBuffer = device.ImportSharedBufferMemory(&desc);
    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = kBufferWidth;
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc |
wgpu::BufferUsage::Storage; wgpu::Buffer buffer = sharedBuffer.CreateBuffer(&bufferDesc);

    // Write shared memory data from within Dawn
    wgpu::SharedBufferMemoryBeginAccessDescriptor beginAccessDesc;
    beginAccessDesc.fenceCount = 0;
    sharedBuffer.BeginAccess(buffer, &beginAccessDesc);
    wgpu::SharedBufferMemoryEndAccessState endAccessState = {};
    EXPECT_BUFFER_U32_EQ(kBufferData, buffer, 0);
    sharedBuffer.EndAccess(buffer, &endAccessState);

    // Read shared memory outside of Dawn
    ASSERT_EQ(ReadBufferDataExternal(), kBufferData);
}*/

DAWN_INSTANTIATE_TEST(D3D12SharedBufferMemoryTests, D3D12Backend());

}  // namespace
}  // namespace dawn
