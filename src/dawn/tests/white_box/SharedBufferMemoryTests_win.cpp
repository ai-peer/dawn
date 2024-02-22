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
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {

constexpr uint32_t kBufferWidth = 32;

class D3D12SharedBufferMemoryTests : public DawnTest {
    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        return {wgpu::FeatureName::SharedBufferMemoryD3D12Resource};
    }

  public:
    ComPtr<ID3D12Resource> CreateD3D12Buffer(D3D12_HEAP_TYPE heapType,
                                             D3D12_RESOURCE_FLAGS resourceFlags) {
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

        mExternalDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &descriptor,
                                                 D3D12_RESOURCE_STATE_COMMON, {},
                                                 IID_PPV_ARGS(&resource));
        return resource;
    }

    void SetUp() override {
        DawnTest::SetUp();
        DAWN_TEST_UNSUPPORTED_IF(UsesWire());

        ComPtr<IDXGIAdapter> dxgiAdapter = native::d3d::GetDXGIAdapter(device.GetAdapter().Get());
        DXGI_ADAPTER_DESC adapterDesc;
        dxgiAdapter->GetDesc(&adapterDesc);

        ComPtr<IDXGIFactory4> dxgiFactory;
        CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory));
        dxgiAdapter = nullptr;
        dxgiFactory->EnumAdapterByLuid(adapterDesc.AdapterLuid, IID_PPV_ARGS(&dxgiAdapter));

        D3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                          &mExternalDevice);

        mExternalDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                IID_PPV_ARGS(&mCommandAllocator));
        mExternalDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           mCommandAllocator.Get(), nullptr,
                                           IID_PPV_ARGS(&mCommandList));

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        mExternalDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue));
    }

    wgpu::SharedBufferMemory CreateSharedBufferMemory(ComPtr<ID3D12Resource> sharedResource) {
        wgpu::SharedBufferMemoryDescriptor desc;
        native::d3d12::SharedBufferMemoryD3D12ResourceDescriptor sharedD3d12ResourceDesc;
        sharedD3d12ResourceDesc.resource = sharedResource.Get();
        desc.nextInChain = &sharedD3d12ResourceDesc;
        return device.ImportSharedBufferMemory(&desc);
    }

  private:
    ComPtr<ID3D12CommandAllocator> mCommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> mCommandList;
    ComPtr<ID3D12CommandQueue> mCommandQueue;
    ComPtr<ID3D12Device> mExternalDevice;
};

TEST_P(D3D12SharedBufferMemoryTests, CreateSharedBuffer) {
    ComPtr<ID3D12Resource> resource =
        CreateD3D12Buffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE);
    wgpu::SharedBufferMemory sharedBuffer = CreateSharedBufferMemory(resource);
    wgpu::Buffer buffer = sharedBuffer.CreateBuffer();
}

DAWN_INSTANTIATE_TEST(D3D12SharedBufferMemoryTests, D3D12Backend());

}  // anonymous namespace
}  // namespace dawn
