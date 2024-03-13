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
#include <vector>
#include "dawn/native/D3D12Backend.h"
#include "dawn/native/d3d12/DeviceD3D12.h"
#include "dawn/tests/DawnTest.h"
#include "dawn/tests/white_box/SharedBufferMemoryTests.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {
constexpr uint32_t kBufferData = 0x76543210;
constexpr uint32_t kBufferWidth = 32;

void WriteUploadBuffer(ID3D12Resource* resource, uint32_t data) {
    void* mappedBufferBegin;
    D3D12_RANGE range;
    range.Begin = 0;
    range.End = 4;
    resource->Map(0, &range, &mappedBufferBegin);
    static_cast<uint32_t*>(mappedBufferBegin)[0] = data;
    resource->Unmap(0, &range);
}

uint32_t readReadbackBuffer(ID3D12Resource* resource) {
    void* mappedBufferBegin;
    D3D12_RANGE range;
    range.Begin = 0;
    range.End = kBufferWidth / 8;
    resource->Map(0, &range, &mappedBufferBegin);
    uint32_t readbackData = static_cast<uint32_t*>(mappedBufferBegin)[0];
    resource->Unmap(0, &range);

    return readbackData;
}

class Backend : public SharedBufferMemoryTestBackend {
  public:
    static Backend* GetInstance() {
        static Backend b;
        return &b;
    }

    std::vector<wgpu::FeatureName> RequiredFeatures(const wgpu::Adapter& adapter) const override {
        return {wgpu::FeatureName::SharedBufferMemoryD3D12Resource,
                wgpu::FeatureName::SharedFenceDXGISharedHandle};
    }

    wgpu::SharedBufferMemory CreateSharedBufferMemory(const wgpu::Device& device) override {
        ComPtr<ID3D12Device> d3d12Device = CreateD3D12Device(device);
        ComPtr<ID3D12Resource> d3d12Resource =
            CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE);

        wgpu::SharedBufferMemoryDescriptor desc;
        native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
        sharedD3d12ResourceDesc.resource = d3d12Resource.Get();
        desc.nextInChain = &sharedD3d12ResourceDesc;
        return device.ImportSharedBufferMemory(&desc);
    }

    ComPtr<ID3D12Device> CreateD3D12Device(const wgpu::Device& device,
                                           bool createWarpDevice = false) {
        ComPtr<IDXGIAdapter> dxgiAdapter;
        ComPtr<IDXGIFactory4> dxgiFactory;
        CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory));
        dxgiAdapter = nullptr;
        if (createWarpDevice) {
            dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter));

        } else {
            dxgiAdapter = native::d3d::GetDXGIAdapter(device.GetAdapter().Get());
            DXGI_ADAPTER_DESC adapterDesc;
            dxgiAdapter->GetDesc(&adapterDesc);
            dxgiFactory->EnumAdapterByLuid(adapterDesc.AdapterLuid, IID_PPV_ARGS(&dxgiAdapter));
        }

        ComPtr<ID3D12Device> d3d12Device;

        D3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                          &d3d12Device);

        return d3d12Device;
    }

    ComPtr<ID3D12Resource> CreateD3D12Buffer(ID3D12Device* device,
                                             D3D12_HEAP_TYPE heapType,
                                             D3D12_RESOURCE_FLAGS resourceFlags) {
        D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COMMON;
        if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
            initialResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
        }

        D3D12_HEAP_PROPERTIES heapProperties = {heapType, D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                                D3D12_MEMORY_POOL_UNKNOWN, 0, 0};

        D3D12_RESOURCE_DESC descriptor;
        descriptor.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        descriptor.Alignment = 0;
        descriptor.Width = kBufferWidth;
        descriptor.Height = 1;
        descriptor.DepthOrArraySize = 1;
        descriptor.MipLevels = 1;
        descriptor.Format = DXGI_FORMAT_UNKNOWN;
        descriptor.SampleDesc.Count = 1;
        descriptor.SampleDesc.Quality = 0;
        descriptor.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        descriptor.Flags = resourceFlags;

        ComPtr<ID3D12Resource> resource;

        device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &descriptor,
                                        initialResourceState, {}, IID_PPV_ARGS(&resource));
        return resource;
    }

  private:
    Backend() {}
};

// Test importing a {UPLOAD, READBACK, DEFAULT} buffer.
TEST_P(SharedBufferMemoryTests, ImportBuffer) {
    for (D3D12_HEAP_TYPE heapType :
         {D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_TYPE_READBACK}) {
        ComPtr<ID3D12Device> d3d12Device =
            static_cast<Backend*>(GetParam().mBackend)->CreateD3D12Device(device);
        ComPtr<ID3D12Resource> d3d12Resource =
            static_cast<Backend*>(GetParam().mBackend)
                ->CreateD3D12Buffer(d3d12Device.Get(), heapType, D3D12_RESOURCE_FLAG_NONE);
        wgpu::SharedBufferMemoryDescriptor desc;
        native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
        sharedD3d12ResourceDesc.resource = d3d12Resource.Get();
        desc.nextInChain = &sharedD3d12ResourceDesc;
        device.ImportSharedBufferMemory(&desc);
    }
}

// Ensure BeginAccess works with a SharedFence.
TEST_P(SharedBufferMemoryTests, BeginAccessSharedFence) {
    ComPtr<ID3D12Device> d3d12Device =
        static_cast<Backend*>(GetParam().mBackend)->CreateD3D12Device(device);
    ComPtr<ID3D12Resource> d3d12UploadBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                D3D12_RESOURCE_FLAG_NONE);
    ComPtr<ID3D12Resource> d3d12DefaultBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_DEFAULT,
                                D3D12_RESOURCE_FLAG_NONE);

    // Import buffer to Dawn.
    wgpu::SharedBufferMemoryDescriptor desc;
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = d3d12DefaultBuffer.Get();
    desc.nextInChain = &sharedD3d12ResourceDesc;
    wgpu::SharedBufferMemory sharedBufferMemory = device.ImportSharedBufferMemory(&desc);
    wgpu::Buffer sharedBuffer = sharedBufferMemory.CreateBuffer();

    // Write data to the buffer outside of Dawn.
    ComPtr<ID3D12CommandAllocator> d3d12CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> d3d12CommandList;
    ComPtr<ID3D12CommandQueue> d3d12CommandQueue;
    d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                        IID_PPV_ARGS(&d3d12CommandAllocator));
    d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12CommandAllocator.Get(),
                                   nullptr, IID_PPV_ARGS(&d3d12CommandList));
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3d12CommandQueue));

    WriteUploadBuffer(d3d12UploadBuffer.Get(), kBufferData);

    ID3D12CommandList* commandLists[] = {d3d12CommandList.Get()};
    d3d12CommandList->CopyResource(d3d12DefaultBuffer.Get(), d3d12UploadBuffer.Get());
    d3d12CommandList->Close();
    d3d12CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    // Create a fence that will signal the data write is complete.
    ComPtr<ID3D12Fence> d3d12Fence;
    d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&d3d12Fence));
    constexpr uint64_t fenceSignalValue = 1;

    // Create and import a shared fence.
    HANDLE fenceSharedHandle;
    d3d12Device->CreateSharedHandle(d3d12Fence.Get(), nullptr, GENERIC_ALL, nullptr,
                                    &fenceSharedHandle);
    wgpu::SharedFenceDXGISharedHandleDescriptor sharedHandleDesc;
    sharedHandleDesc.handle = fenceSharedHandle;
    wgpu::SharedFenceDescriptor fenceDesc;
    fenceDesc.nextInChain = &sharedHandleDesc;
    wgpu::SharedFence sharedFence = device.ImportSharedFence(&fenceDesc);

    // Begin access with Dawn
    wgpu::SharedBufferMemoryBeginAccessDescriptor beginAccessDesc;
    beginAccessDesc.fenceCount = 1;
    beginAccessDesc.signaledValues = &fenceSignalValue;
    beginAccessDesc.fences = &sharedFence;
    beginAccessDesc.initialized = true;
    sharedBufferMemory.BeginAccess(sharedBuffer, &beginAccessDesc);

    // Signal the shared fence to ensure the external copy has finished.
    d3d12CommandQueue->Signal(d3d12Fence.Get(), fenceSignalValue);

    // Check the buffer data with Dawn.
    EXPECT_BUFFER_U32_EQ(kBufferData, sharedBuffer, 0);
}

// Ensure EndAccess works with a SharedFence.
TEST_P(SharedBufferMemoryTests, EndAccessSharedFence) {
    ComPtr<ID3D12Device> d3d12Device =
        static_cast<Backend*>(GetParam().mBackend)->CreateD3D12Device(device);
    ComPtr<ID3D12Resource> d3d12ReadbackBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_READBACK,
                                D3D12_RESOURCE_FLAG_NONE);

    // Import buffer to Dawn and copy new data.
    wgpu::SharedBufferMemoryDescriptor desc;
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = d3d12ReadbackBuffer.Get();
    desc.nextInChain = &sharedD3d12ResourceDesc;
    wgpu::SharedBufferMemory sharedBufferMemory = device.ImportSharedBufferMemory(&desc);
    wgpu::Buffer sharedBuffer = sharedBufferMemory.CreateBuffer();

    wgpu::SharedBufferMemoryBeginAccessDescriptor beginAccessDesc;
    beginAccessDesc.fenceCount = 0;
    beginAccessDesc.initialized = false;
    sharedBufferMemory.BeginAccess(sharedBuffer, &beginAccessDesc);

    // Copy data into the upload buffer from within Dawn.
    wgpu::Buffer dawnBuffer = utils::CreateBufferFromData(device, &kBufferData, kBufferWidth / 8,
                                                          wgpu::BufferUsage::CopySrc);
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToBuffer(dawnBuffer, 0, sharedBuffer, 0, 4);
    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);

    wgpu::SharedBufferMemoryEndAccessState endAccessState = {};
    sharedBufferMemory.EndAccess(sharedBuffer, &endAccessState);

    wgpu::SharedFence dawnFence = endAccessState.fences[0];
    wgpu::SharedFenceExportInfo exportInfo;
    wgpu::SharedFenceDXGISharedHandleExportInfo dxgiExportInfo;
    exportInfo.nextInChain = &dxgiExportInfo;
    dawnFence.ExportInfo(&exportInfo);

    HANDLE dawnFenceHandle = dxgiExportInfo.handle;

    ComPtr<ID3D12Fence> d3d12SharedFence;
    d3d12Device->OpenSharedHandle(dawnFenceHandle, IID_PPV_ARGS(&d3d12SharedFence));

    HANDLE fenceEvent = 0;

    // Wait until Dawn's exported fence is finished.
    if (d3d12SharedFence->GetCompletedValue() <= endAccessState.signaledValues[0]) {
        d3d12SharedFence->SetEventOnCompletion(endAccessState.signaledValues[0] + 1, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    // Map and read data outside of Dawn.
    ASSERT_EQ(readReadbackBuffer(d3d12ReadbackBuffer.Get()), kBufferData);
}

// Perform a read operation on a shared UPLOAD buffer within Dawn.
TEST_P(SharedBufferMemoryTests, ReadUploadBuffer) {
    ComPtr<ID3D12Device> d3d12Device =
        static_cast<Backend*>(GetParam().mBackend)->CreateD3D12Device(device);
    ComPtr<ID3D12Resource> d3d12UploadBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                D3D12_RESOURCE_FLAG_NONE);

    WriteUploadBuffer(d3d12UploadBuffer.Get(), kBufferData);

    wgpu::SharedBufferMemoryDescriptor desc;
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = d3d12UploadBuffer.Get();
    desc.nextInChain = &sharedD3d12ResourceDesc;

    // Import buffer to Dawn and copy contents into a dawn buffer with read access.
    wgpu::SharedBufferMemory sharedBufferMemory = device.ImportSharedBufferMemory(&desc);
    wgpu::Buffer sharedBuffer = sharedBufferMemory.CreateBuffer();

    wgpu::BufferDescriptor descriptor;
    descriptor.size = kBufferWidth / 8;
    descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc;
    wgpu::Buffer dawnBuffer = device.CreateBuffer(&descriptor);

    wgpu::SharedBufferMemoryBeginAccessDescriptor beginAccessDesc;
    beginAccessDesc.fenceCount = 0;
    beginAccessDesc.initialized = true;
    sharedBufferMemory.BeginAccess(sharedBuffer, &beginAccessDesc);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToBuffer(sharedBuffer, 0, dawnBuffer, 0, kBufferWidth / 8);
    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);

    // Dawn buffer should contain data originally written to sharedBuffer.
    EXPECT_BUFFER_U32_EQ(kBufferData, dawnBuffer, 0);

    wgpu::SharedBufferMemoryEndAccessState endAccessState = {};
    sharedBufferMemory.EndAccess(sharedBuffer, &endAccessState);
}

// Perform a write operation on a shared UPLOAD buffer within Dawn.
TEST_P(SharedBufferMemoryTests, WriteUploadBuffer) {
    ComPtr<ID3D12Device> d3d12Device =
        static_cast<Backend*>(GetParam().mBackend)->CreateD3D12Device(device);
    ComPtr<ID3D12Resource> d3d12UploadBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                D3D12_RESOURCE_FLAG_NONE);

    wgpu::SharedBufferMemoryDescriptor desc;
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = d3d12UploadBuffer.Get();
    desc.nextInChain = &sharedD3d12ResourceDesc;

    // Import buffer into Dawn. Map and write data into the buffer.
    wgpu::SharedBufferMemory sharedBufferMemory = device.ImportSharedBufferMemory(&desc);
    wgpu::Buffer sharedBuffer = sharedBufferMemory.CreateBuffer();

    wgpu::SharedBufferMemoryBeginAccessDescriptor beginAccessDesc;
    beginAccessDesc.fenceCount = 0;
    beginAccessDesc.initialized = false;
    sharedBufferMemory.BeginAccess(sharedBuffer, &beginAccessDesc);

    bool done = false;
    sharedBuffer.MapAsync(
        wgpu::MapMode::Write, 0, kBufferWidth / 8,
        [](WGPUBufferMapAsyncStatus status, void* userdata) {
            ASSERT_EQ(WGPUBufferMapAsyncStatus_Success, status);
            *static_cast<bool*>(userdata) = true;
        },
        &done);

    while (!done) {
        WaitABit();
    }

    uint32_t* mappedData = static_cast<uint32_t*>(sharedBuffer.GetMappedRange(0, kBufferWidth / 8));
    memcpy(mappedData, &kBufferData, 4);

    sharedBuffer.Unmap();

    wgpu::SharedBufferMemoryEndAccessState state;
    sharedBufferMemory.EndAccess(sharedBuffer, &state);

    // Copy data into a readback buffer using D3D12 to verify contents
    ComPtr<ID3D12CommandAllocator> d3d12CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> d3d12CommandList;
    ComPtr<ID3D12CommandQueue> d3d12CommandQueue;
    d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                        IID_PPV_ARGS(&d3d12CommandAllocator));
    d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12CommandAllocator.Get(),
                                   nullptr, IID_PPV_ARGS(&d3d12CommandList));
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3d12CommandQueue));

    ComPtr<ID3D12Resource> d3d12ReadbackBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_READBACK,
                                D3D12_RESOURCE_FLAG_NONE);

    ID3D12CommandList* commandLists[] = {d3d12CommandList.Get()};
    d3d12CommandList->CopyResource(d3d12ReadbackBuffer.Get(), d3d12UploadBuffer.Get());
    d3d12CommandList->Close();
    d3d12CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    ComPtr<ID3D12Fence> d3d12Fence;
    d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12Fence));
    constexpr uint32_t signalValue = 1;
    d3d12CommandQueue->Signal(d3d12Fence.Get(), signalValue);
    HANDLE fenceEvent = 0;

    // Wait until the previous operation is finished.
    if (d3d12Fence->GetCompletedValue() < signalValue) {
        d3d12Fence->SetEventOnCompletion(signalValue, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    // Map and read data
    ASSERT_EQ(readReadbackBuffer(d3d12ReadbackBuffer.Get()), kBufferData);
}

// Perform a read operation on a shared DEFAULT buffer within Dawn.
TEST_P(SharedBufferMemoryTests, ReadDefaultBuffer) {
    ComPtr<ID3D12Device> d3d12Device =
        static_cast<Backend*>(GetParam().mBackend)->CreateD3D12Device(device);
    ComPtr<ID3D12Resource> d3d12UploadBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                D3D12_RESOURCE_FLAG_NONE);
    ComPtr<ID3D12Resource> d3d12DefaultBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_DEFAULT,
                                D3D12_RESOURCE_FLAG_NONE);

    ComPtr<ID3D12CommandAllocator> d3d12CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> d3d12CommandList;
    ComPtr<ID3D12CommandQueue> d3d12CommandQueue;
    d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                        IID_PPV_ARGS(&d3d12CommandAllocator));
    d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12CommandAllocator.Get(),
                                   nullptr, IID_PPV_ARGS(&d3d12CommandList));
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3d12CommandQueue));

    WriteUploadBuffer(d3d12UploadBuffer.Get(), kBufferData);

    // Copy from UPLOAD buffer to DEFAULT buffer outside Dawn.
    ID3D12CommandList* commandLists[] = {d3d12CommandList.Get()};
    d3d12CommandList->CopyResource(d3d12DefaultBuffer.Get(), d3d12UploadBuffer.Get());
    d3d12CommandList->Close();

    d3d12CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    HANDLE m_fenceEvent = 0;
    ComPtr<ID3D12Fence> mFence;
    d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));
    UINT64 fence = 0;
    d3d12CommandQueue->Signal(mFence.Get(), 1);
    fence++;

    // Wait until the previous operation is finished.
    if (mFence->GetCompletedValue() == 0) {
        mFence->SetEventOnCompletion(fence, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Import buffer to Dawn and verify contents.
    wgpu::SharedBufferMemoryDescriptor desc;
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = d3d12DefaultBuffer.Get();
    desc.nextInChain = &sharedD3d12ResourceDesc;
    wgpu::SharedBufferMemory sharedBufferMemory = device.ImportSharedBufferMemory(&desc);
    wgpu::Buffer sharedBuffer = sharedBufferMemory.CreateBuffer();

    wgpu::SharedBufferMemoryBeginAccessDescriptor beginAccessDesc;
    beginAccessDesc.fenceCount = 0;
    beginAccessDesc.initialized = true;
    sharedBufferMemory.BeginAccess(sharedBuffer, &beginAccessDesc);

    EXPECT_BUFFER_U32_EQ(kBufferData, sharedBuffer, 0);

    wgpu::SharedBufferMemoryEndAccessState endAccessState = {};
    sharedBufferMemory.EndAccess(sharedBuffer, &endAccessState);
}

// Perform a write operation on a shared DEFAULT buffer within Dawn.
TEST_P(SharedBufferMemoryTests, WriteDefaultBuffer) {
    ComPtr<ID3D12Device> d3d12Device =
        static_cast<Backend*>(GetParam().mBackend)->CreateD3D12Device(device);
    ComPtr<ID3D12Resource> d3d12DefaultBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_DEFAULT,
                                D3D12_RESOURCE_FLAG_NONE);

    // Import buffer to Dawn and copy new data.
    wgpu::SharedBufferMemoryDescriptor desc;
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = d3d12DefaultBuffer.Get();
    desc.nextInChain = &sharedD3d12ResourceDesc;
    wgpu::SharedBufferMemory sharedBufferMemory = device.ImportSharedBufferMemory(&desc);
    wgpu::Buffer sharedBuffer = sharedBufferMemory.CreateBuffer();

    wgpu::SharedBufferMemoryBeginAccessDescriptor beginAccessDesc;
    beginAccessDesc.fenceCount = 0;
    beginAccessDesc.initialized = false;
    sharedBufferMemory.BeginAccess(sharedBuffer, &beginAccessDesc);

    // Copy different data into the upload buffer from within Dawn.
    wgpu::Buffer dawnBuffer = utils::CreateBufferFromData(device, &kBufferData, kBufferWidth / 8,
                                                          wgpu::BufferUsage::CopySrc);
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToBuffer(dawnBuffer, 0, sharedBuffer, 0, 4);
    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);

    wgpu::SharedBufferMemoryEndAccessState endAccessState = {};
    sharedBufferMemory.EndAccess(sharedBuffer, &endAccessState);

    ComPtr<ID3D12Resource> d3d12ReadbackBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_READBACK,
                                D3D12_RESOURCE_FLAG_NONE);
    ComPtr<ID3D12CommandAllocator> d3d12CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> d3d12CommandList;
    ComPtr<ID3D12CommandQueue> d3d12CommandQueue;
    d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                        IID_PPV_ARGS(&d3d12CommandAllocator));
    d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12CommandAllocator.Get(),
                                   nullptr, IID_PPV_ARGS(&d3d12CommandList));
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3d12CommandQueue));

    ID3D12CommandList* commandLists[] = {d3d12CommandList.Get()};
    d3d12CommandList->CopyResource(d3d12ReadbackBuffer.Get(), d3d12DefaultBuffer.Get());
    d3d12CommandList->Close();
    d3d12CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    HANDLE m_fenceEvent2 = 0;
    ComPtr<ID3D12Fence> d3d12Fence2;
    d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12Fence2));
    UINT64 fence2 = 0;
    d3d12CommandQueue->Signal(d3d12Fence2.Get(), 1);
    fence2++;

    // Wait until the previous operation is finished.
    if (d3d12Fence2->GetCompletedValue() == 0) {
        d3d12Fence2->SetEventOnCompletion(fence2, m_fenceEvent2);
        WaitForSingleObject(m_fenceEvent2, INFINITE);
    }

    // Map and read data
    ASSERT_EQ(readReadbackBuffer(d3d12ReadbackBuffer.Get()), kBufferData);
}

// Perform a read operation on a shared READBACK buffer within Dawn.
TEST_P(SharedBufferMemoryTests, ReadReadbackBuffer) {
    ComPtr<ID3D12Device> d3d12Device =
        static_cast<Backend*>(GetParam().mBackend)->CreateD3D12Device(device);
    ComPtr<ID3D12Resource> d3d12ReadbackBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_READBACK,
                                D3D12_RESOURCE_FLAG_NONE);
    ComPtr<ID3D12Resource> d3d12UploadBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                D3D12_RESOURCE_FLAG_NONE);

    ComPtr<ID3D12CommandAllocator> d3d12CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> d3d12CommandList;
    ComPtr<ID3D12CommandQueue> d3d12CommandQueue;
    d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                        IID_PPV_ARGS(&d3d12CommandAllocator));
    d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12CommandAllocator.Get(),
                                   nullptr, IID_PPV_ARGS(&d3d12CommandList));
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3d12CommandQueue));

    WriteUploadBuffer(d3d12UploadBuffer.Get(), kBufferData);

    // Copy from UPLOAD buffer to READBACK buffer outside Dawn.
    ID3D12CommandList* commandLists[] = {d3d12CommandList.Get()};
    d3d12CommandList->CopyResource(d3d12ReadbackBuffer.Get(), d3d12UploadBuffer.Get());
    d3d12CommandList->Close();

    d3d12CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    HANDLE m_fenceEvent = 0;
    ComPtr<ID3D12Fence> mFence;
    d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));
    UINT64 fence = 0;
    d3d12CommandQueue->Signal(mFence.Get(), 1);
    fence++;

    // Wait until the previous operation is finished.
    if (mFence->GetCompletedValue() == 0) {
        mFence->SetEventOnCompletion(fence, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Import buffer to Dawn. Map the buffer and read the contents.
    wgpu::SharedBufferMemoryDescriptor desc;
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = d3d12ReadbackBuffer.Get();
    desc.nextInChain = &sharedD3d12ResourceDesc;
    wgpu::SharedBufferMemory sharedBufferMemory = device.ImportSharedBufferMemory(&desc);
    wgpu::Buffer sharedBuffer = sharedBufferMemory.CreateBuffer();

    wgpu::SharedBufferMemoryBeginAccessDescriptor beginAccessDesc;
    beginAccessDesc.fenceCount = 0;
    beginAccessDesc.initialized = true;
    sharedBufferMemory.BeginAccess(sharedBuffer, &beginAccessDesc);

    bool done = false;
    sharedBuffer.MapAsync(
        wgpu::MapMode::Read, 0, kBufferWidth / 8,
        [](WGPUBufferMapAsyncStatus status, void* userdata) {
            ASSERT_EQ(WGPUBufferMapAsyncStatus_Success, status);
            *static_cast<bool*>(userdata) = true;
        },
        &done);

    while (!done) {
        WaitABit();
    }

    const uint32_t* mappedData =
        static_cast<const uint32_t*>(sharedBuffer.GetConstMappedRange(0, kBufferWidth / 8));
    ASSERT_EQ(*mappedData, kBufferData);
}

// Perform a write operation on a shared READBACK buffer within Dawn.
TEST_P(SharedBufferMemoryTests, WriteReadbackBuffer) {
    ComPtr<ID3D12Device> d3d12Device =
        static_cast<Backend*>(GetParam().mBackend)->CreateD3D12Device(device);
    ComPtr<ID3D12Resource> d3d12ReadbackBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_READBACK,
                                D3D12_RESOURCE_FLAG_NONE);

    // Import buffer to Dawn and copy new data.
    wgpu::SharedBufferMemoryDescriptor desc;
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = d3d12ReadbackBuffer.Get();
    desc.nextInChain = &sharedD3d12ResourceDesc;
    wgpu::SharedBufferMemory sharedBufferMemory = device.ImportSharedBufferMemory(&desc);
    wgpu::Buffer sharedBuffer = sharedBufferMemory.CreateBuffer();

    wgpu::SharedBufferMemoryBeginAccessDescriptor beginAccessDesc;
    beginAccessDesc.fenceCount = 0;
    beginAccessDesc.initialized = false;
    sharedBufferMemory.BeginAccess(sharedBuffer, &beginAccessDesc);

    // Copy data into the upload buffer from within Dawn.
    wgpu::Buffer dawnBuffer = utils::CreateBufferFromData(device, &kBufferData, kBufferWidth / 8,
                                                          wgpu::BufferUsage::CopySrc);
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToBuffer(dawnBuffer, 0, sharedBuffer, 0, 4);
    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);

    wgpu::SharedBufferMemoryEndAccessState endAccessState = {};
    sharedBufferMemory.EndAccess(sharedBuffer, &endAccessState);

    wgpu::SharedFence dawnFence = endAccessState.fences[0];
    wgpu::SharedFenceExportInfo exportInfo;
    wgpu::SharedFenceDXGISharedHandleExportInfo dxgiExportInfo;
    exportInfo.nextInChain = &dxgiExportInfo;
    dawnFence.ExportInfo(&exportInfo);

    HANDLE dawnFenceHandle = dxgiExportInfo.handle;

    ComPtr<ID3D12Fence> d3d12SharedFence;
    d3d12Device->OpenSharedHandle(dawnFenceHandle, IID_PPV_ARGS(&d3d12SharedFence));

    HANDLE fenceEvent = 0;
    WaitForSingleObject(fenceEvent, INFINITE);
    // Wait until Dawn's exported fence is finished.
    if (d3d12SharedFence->GetCompletedValue() <= endAccessState.signaledValues[0]) {
        d3d12SharedFence->SetEventOnCompletion(endAccessState.signaledValues[0] + 1, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    // Map and read data outside of Dawn.
    ASSERT_EQ(readReadbackBuffer(d3d12ReadbackBuffer.Get()), kBufferData);
}

// Ensure that importing a nullptr ID3D12Resource results in error.
TEST_P(SharedBufferMemoryTests, nullResourceFailure) {
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = nullptr;
    wgpu::SharedBufferMemoryDescriptor desc;
    desc.nextInChain = &sharedD3d12ResourceDesc;
    ASSERT_DEVICE_ERROR(device.ImportSharedBufferMemory(&desc));
}

// Validate that importing an ID3D12Resource across devices results in failure. This is tested by
// creating a resource with a WARP device and attempting to use it on a non-WARP device.
TEST_P(SharedBufferMemoryTests, CrossDeviceResourceImportFailure) {
    ComPtr<ID3D12Device> warpDevice =
        static_cast<Backend*>(GetParam().mBackend)->CreateD3D12Device(device, true);
    ComPtr<ID3D12Resource> d3d12Resource =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(warpDevice.Get(), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE);
    wgpu::SharedBufferMemoryDescriptor desc;
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = d3d12Resource.Get();
    desc.nextInChain = &sharedD3d12ResourceDesc;
    if (IsWARP()) {
        device.ImportSharedBufferMemory(&desc);
    } else {
        ASSERT_DEVICE_ERROR(device.ImportSharedBufferMemory(&desc));
    }
}

DAWN_INSTANTIATE_PREFIXED_TEST_P(D3D12,
                                 SharedBufferMemoryTests,
                                 {D3D12Backend()},
                                 {Backend::GetInstance()});

}  // anonymous namespace
}  // namespace dawn
