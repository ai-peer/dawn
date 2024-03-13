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
constexpr uint32_t kBufferSize = 4;

struct FenceInfo {
    ComPtr<ID3D12Fence> fence;
    uint64_t signaledValue;
};

void WriteD3D12UploadBuffer(ID3D12Resource* resource, uint32_t data) {
    void* mappedBufferBegin;
    D3D12_RANGE range;
    range.Begin = 0;
    range.End = kBufferSize;
    resource->Map(0, &range, &mappedBufferBegin);
    memcpy(mappedBufferBegin, &data, kBufferSize);
    resource->Unmap(0, &range);
}

uint32_t ReadReadbackBuffer(ID3D12Resource* readbackBuffer) {
    void* mappedBufferBegin;
    D3D12_RANGE range;
    range.Begin = 0;
    range.End = kBufferSize;
    readbackBuffer->Map(0, &range, &mappedBufferBegin);
    uint32_t readbackData = static_cast<uint32_t*>(mappedBufferBegin)[0];
    readbackBuffer->Unmap(0, nullptr);

    return readbackData;
}

ComPtr<ID3D12CommandQueue> CreateD3D12CommandQueue(ID3D12Device* device) {
    ComPtr<ID3D12CommandQueue> commandQueue;
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    return commandQueue;
}

FenceInfo CopyD3D12Resource(ID3D12Device* device,
                            ID3D12CommandQueue* commandQueue,
                            ID3D12Resource* source,
                            ID3D12Resource* destination) {
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr,
                              IID_PPV_ARGS(&commandList));

    ID3D12CommandList* commandLists[] = {commandList.Get()};
    commandList->CopyResource(destination, source);
    commandList->Close();

    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&fence));
    UINT64 signaledValue = 1;
    commandQueue->Signal(fence.Get(), signaledValue);

    FenceInfo fenceInfo;
    fenceInfo.fence = fence;
    fenceInfo.signaledValue = signaledValue;
    return fenceInfo;
}

void WaitOnD3D12Fence(FenceInfo fenceInfo) {
    HANDLE fenceEvent = 0;
    if (fenceInfo.fence->GetCompletedValue() < fenceInfo.signaledValue) {
        fenceInfo.fence->SetEventOnCompletion(fenceInfo.signaledValue, fenceEvent);
        WaitForSingleObject(fenceEvent, 1000);
    }
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

    wgpu::SharedBufferMemory CreateSharedBufferMemory(const wgpu::Device& device,
                                                      uint32_t bufferSize) override {
        ComPtr<ID3D12Device> d3d12Device = CreateD3D12Device(device);
        ComPtr<ID3D12Resource> d3d12Resource = CreateD3D12Buffer(
            d3d12Device.Get(), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, bufferSize);

        wgpu::SharedBufferMemoryDescriptor desc;
        native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
        sharedD3d12ResourceDesc.resource = d3d12Resource.Get();
        desc.nextInChain = &sharedD3d12ResourceDesc;
        return device.ImportSharedBufferMemory(&desc);
    }

    ComPtr<ID3D12Device> CreateD3D12Device(const wgpu::Device& device,
                                           bool createWarpDevice = false) {
        ComPtr<IDXGIAdapter> dxgiAdapter = nullptr;
        ComPtr<IDXGIFactory4> dxgiFactory;
        CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory));
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
                                             D3D12_RESOURCE_FLAGS resourceFlags,
                                             uint32_t bufferSize = kBufferSize) {
        D3D12_RESOURCE_STATES initialResourceState;
        switch (heapType) {
            case D3D12_HEAP_TYPE_UPLOAD:
                initialResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
                break;
            case D3D12_HEAP_TYPE_READBACK:
                initialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
                break;
            default:
                initialResourceState = D3D12_RESOURCE_STATE_COMMON;
        }

        D3D12_HEAP_PROPERTIES heapProperties = {heapType, D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                                D3D12_MEMORY_POOL_UNKNOWN, 0, 0};

        D3D12_RESOURCE_DESC descriptor;
        descriptor.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        descriptor.Alignment = 0;
        descriptor.Width = bufferSize * 8;
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

// Perform a read operation on a shared UPLOAD buffer within Dawn.
TEST_P(SharedBufferMemoryTests, ReadUploadBuffer) {
    ComPtr<ID3D12Device> d3d12Device =
        static_cast<Backend*>(GetParam().mBackend)->CreateD3D12Device(device);
    ComPtr<ID3D12Resource> d3d12UploadBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                D3D12_RESOURCE_FLAG_NONE);

    WriteD3D12UploadBuffer(d3d12UploadBuffer.Get(), kBufferData);

    wgpu::SharedBufferMemoryDescriptor desc;
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = d3d12UploadBuffer.Get();
    desc.nextInChain = &sharedD3d12ResourceDesc;

    // Import buffer to Dawn and copy contents into a dawn buffer with read access.
    wgpu::SharedBufferMemory sharedBufferMemory = device.ImportSharedBufferMemory(&desc);
    wgpu::Buffer sharedBuffer = sharedBufferMemory.CreateBuffer();

    wgpu::BufferDescriptor descriptor;
    descriptor.size = kBufferSize;
    descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc;
    wgpu::Buffer dawnBuffer = device.CreateBuffer(&descriptor);

    wgpu::SharedBufferMemoryBeginAccessDescriptor beginAccessDesc;
    beginAccessDesc.initialized = true;
    sharedBufferMemory.BeginAccess(sharedBuffer, &beginAccessDesc);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToBuffer(sharedBuffer, 0, dawnBuffer, 0, kBufferSize);
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
    beginAccessDesc.initialized = false;
    sharedBufferMemory.BeginAccess(sharedBuffer, &beginAccessDesc);

    bool done = false;
    sharedBuffer.MapAsync(
        wgpu::MapMode::Write, 0, kBufferSize,
        [](WGPUBufferMapAsyncStatus status, void* userdata) {
            ASSERT_EQ(WGPUBufferMapAsyncStatus_Success, status);
            *static_cast<bool*>(userdata) = true;
        },
        &done);

    while (!done) {
        WaitABit();
    }

    uint32_t* mappedData = static_cast<uint32_t*>(sharedBuffer.GetMappedRange(0, kBufferSize));
    memcpy(mappedData, &kBufferData, kBufferSize);

    sharedBuffer.Unmap();

    wgpu::SharedBufferMemoryEndAccessState state;
    sharedBufferMemory.EndAccess(sharedBuffer, &state);

    // Copy the buffer data into a readback buffer to verify the contents.
    ComPtr<ID3D12Resource> d3d12ReadbackBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_READBACK,
                                D3D12_RESOURCE_FLAG_NONE);
    ComPtr<ID3D12CommandQueue> d3d12CommandQueue = CreateD3D12CommandQueue(d3d12Device.Get());
    WaitOnD3D12Fence(CopyD3D12Resource(d3d12Device.Get(), d3d12CommandQueue.Get(),
                                       d3d12UploadBuffer.Get(), d3d12ReadbackBuffer.Get()));

    // Map and read data
    ASSERT_EQ(ReadReadbackBuffer(d3d12ReadbackBuffer.Get()), kBufferData);
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
    ComPtr<ID3D12CommandQueue> d3d12CommandQueue = CreateD3D12CommandQueue(d3d12Device.Get());

    // Upload data through an UPLOAD buffer and copy to the DEFAULT buffer.
    WriteD3D12UploadBuffer(d3d12UploadBuffer.Get(), kBufferData);
    FenceInfo fenceInfo = CopyD3D12Resource(d3d12Device.Get(), d3d12CommandQueue.Get(),
                                            d3d12UploadBuffer.Get(), d3d12DefaultBuffer.Get());

    // Create and import a shared fence.
    HANDLE fenceSharedHandle;
    d3d12Device->CreateSharedHandle(fenceInfo.fence.Get(), nullptr, GENERIC_ALL, nullptr,
                                    &fenceSharedHandle);
    wgpu::SharedFenceDXGISharedHandleDescriptor sharedHandleDesc;
    sharedHandleDesc.handle = fenceSharedHandle;
    wgpu::SharedFenceDescriptor fenceDesc;
    fenceDesc.nextInChain = &sharedHandleDesc;
    wgpu::SharedFence sharedFence = device.ImportSharedFence(&fenceDesc);

    // Import buffer to Dawn and verify contents.
    wgpu::SharedBufferMemoryDescriptor desc;
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = d3d12DefaultBuffer.Get();
    desc.nextInChain = &sharedD3d12ResourceDesc;
    wgpu::SharedBufferMemory sharedBufferMemory = device.ImportSharedBufferMemory(&desc);
    wgpu::Buffer sharedBuffer = sharedBufferMemory.CreateBuffer();
    wgpu::SharedBufferMemoryBeginAccessDescriptor beginAccessDesc;
    beginAccessDesc.initialized = true;
    beginAccessDesc.fenceCount = 1;
    beginAccessDesc.fences = &sharedFence;
    beginAccessDesc.signaledValues = &fenceInfo.signaledValue;
    sharedBufferMemory.BeginAccess(sharedBuffer, &beginAccessDesc);

    EXPECT_BUFFER_U32_EQ(kBufferData, sharedBuffer, 0);
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
    beginAccessDesc.initialized = false;
    sharedBufferMemory.BeginAccess(sharedBuffer, &beginAccessDesc);

    // Copy data into the buffer from within Dawn.
    wgpu::Buffer dawnBuffer =
        utils::CreateBufferFromData(device, &kBufferData, kBufferSize, wgpu::BufferUsage::CopySrc);
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToBuffer(dawnBuffer, 0, sharedBuffer, 0, kBufferSize);
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
    if (d3d12SharedFence->GetCompletedValue() < endAccessState.signaledValues[0]) {
        d3d12SharedFence->SetEventOnCompletion(endAccessState.signaledValues[0], fenceEvent);
        WaitForSingleObject(fenceEvent, 1000);
    }

    ComPtr<ID3D12Resource> d3d12ReadbackBuffer =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(d3d12Device.Get(), D3D12_HEAP_TYPE_READBACK,
                                D3D12_RESOURCE_FLAG_NONE);
    ComPtr<ID3D12CommandQueue> d3d12CommandQueue = CreateD3D12CommandQueue(d3d12Device.Get());

    // Copy the DEFAULT buffer data to a READBACK buffer to map and verify the contents.
    WaitOnD3D12Fence(CopyD3D12Resource(d3d12Device.Get(), d3d12CommandQueue.Get(),
                                       d3d12DefaultBuffer.Get(), d3d12ReadbackBuffer.Get()));
    ASSERT_EQ(ReadReadbackBuffer(d3d12ReadbackBuffer.Get()), kBufferData);
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
    ComPtr<ID3D12CommandQueue> d3d12CommandQueue = CreateD3D12CommandQueue(d3d12Device.Get());

    // Upload data through an UPLOAD buffer and copy to the READBACK buffer.
    WriteD3D12UploadBuffer(d3d12UploadBuffer.Get(), kBufferData);
    WaitOnD3D12Fence(CopyD3D12Resource(d3d12Device.Get(), d3d12CommandQueue.Get(),
                                       d3d12UploadBuffer.Get(), d3d12ReadbackBuffer.Get()));

    // Import buffer to Dawn. Map the buffer and read the contents.
    wgpu::SharedBufferMemoryDescriptor desc;
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = d3d12ReadbackBuffer.Get();
    desc.nextInChain = &sharedD3d12ResourceDesc;
    wgpu::SharedBufferMemory sharedBufferMemory = device.ImportSharedBufferMemory(&desc);
    wgpu::Buffer sharedBuffer = sharedBufferMemory.CreateBuffer();

    wgpu::SharedBufferMemoryBeginAccessDescriptor beginAccessDesc;
    beginAccessDesc.initialized = true;
    sharedBufferMemory.BeginAccess(sharedBuffer, &beginAccessDesc);

    bool done = false;
    sharedBuffer.MapAsync(
        wgpu::MapMode::Read, 0, kBufferSize,
        [](WGPUBufferMapAsyncStatus status, void* userdata) {
            ASSERT_EQ(WGPUBufferMapAsyncStatus_Success, status);
            *static_cast<bool*>(userdata) = true;
        },
        &done);

    while (!done) {
        WaitABit();
    }

    const uint32_t* mappedData =
        static_cast<const uint32_t*>(sharedBuffer.GetConstMappedRange(0, kBufferSize));
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
    beginAccessDesc.initialized = false;
    sharedBufferMemory.BeginAccess(sharedBuffer, &beginAccessDesc);

    // Copy data into the readback buffer from within Dawn.
    wgpu::Buffer dawnBuffer =
        utils::CreateBufferFromData(device, &kBufferData, kBufferSize, wgpu::BufferUsage::CopySrc);
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
    if (d3d12SharedFence->GetCompletedValue() < endAccessState.signaledValues[0]) {
        d3d12SharedFence->SetEventOnCompletion(endAccessState.signaledValues[0], fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    // Map and read data outside of Dawn.
    ASSERT_EQ(ReadReadbackBuffer(d3d12ReadbackBuffer.Get()), kBufferData);
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
    DAWN_TEST_UNSUPPORTED_IF(IsWARP());
    ComPtr<ID3D12Device> warpDevice =
        static_cast<Backend*>(GetParam().mBackend)->CreateD3D12Device(device, true);
    ComPtr<ID3D12Resource> d3d12Resource =
        static_cast<Backend*>(GetParam().mBackend)
            ->CreateD3D12Buffer(warpDevice.Get(), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE);
    wgpu::SharedBufferMemoryDescriptor desc;
    native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
    sharedD3d12ResourceDesc.resource = d3d12Resource.Get();
    desc.nextInChain = &sharedD3d12ResourceDesc;

    ASSERT_DEVICE_ERROR(device.ImportSharedBufferMemory(&desc));
}

DAWN_INSTANTIATE_PREFIXED_TEST_P(D3D12,
                                 SharedBufferMemoryTests,
                                 {D3D12Backend()},
                                 {Backend::GetInstance()});

}  // anonymous namespace
}  // namespace dawn
