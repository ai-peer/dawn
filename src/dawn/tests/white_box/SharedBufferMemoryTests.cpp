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
#include <dxgi1_4.h>
#include <memory>
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
class D3D12SharedBufferMemoryTests : public DawnTest {
    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        return {wgpu::FeatureName::SharedBufferMemoryDXGISharedHandle,
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

        ::D3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                            &mExternalDevice);

        // Create a buffer meant for sharing
        D3D12_HEAP_PROPERTIES heapProperties = {D3D12_HEAP_TYPE_DEFAULT,
                                                D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                                D3D12_MEMORY_POOL_UNKNOWN, 0, 0};

        D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_SHARED;

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

        D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COMMON;
        ComPtr<ID3D12Resource> committedResource;
        mExternalDevice->CreateCommittedResource(&heapProperties, heapFlags, &resourceDescriptor,
                                                 initialResourceState, {},
                                                 IID_PPV_ARGS(&committedResource));
        SECURITY_ATTRIBUTES secAttributes;
        mExternalDevice->CreateSharedHandle(committedResource.Get(), &secAttributes, GENERIC_ALL,
                                            nullptr, &mDXGISharedHandle);
    }

  public:
    HANDLE mDXGISharedHandle;
    ComPtr<ID3D12Device> mExternalDevice;
};

TEST_P(D3D12SharedBufferMemoryTests, CreateSharedBufferMemory) {
    wgpu::SharedBufferMemoryDescriptor desc;
    wgpu::SharedBuffer dxgiDesc;
    dxgiDesc.handle = mDXGISharedHandle;
    desc.nextInChain = &dxgiDesc;
    wgpu::SharedBufferMemory buffer = device.ImportSharedBufferMemory(&desc);
}

DAWN_INSTANTIATE_TEST(D3D12SharedBufferMemoryTests, D3D12Backend());

}  // namespace
}  // namespace dawn
