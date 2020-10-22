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

#include "tests/DawnTest.h"

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include "dawn_native/D3D12Backend.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

using Microsoft::WRL::ComPtr;

constexpr static uint32_t kRTSize = 8;

namespace {
    class VideoViewsTest : public DawnTest {
      public:
        void SetUp() override {
            DawnTest::SetUp();
            DAWN_SKIP_TEST_IF(UsesWire());

            // Create the D3D11 device/contexts that will be used in subsequent tests
            ComPtr<ID3D12Device> d3d12Device = dawn_native::d3d12::GetD3D12Device(device.Get());

            const LUID adapterLuid = d3d12Device->GetAdapterLuid();

            ComPtr<IDXGIFactory4> dxgiFactory;
            HRESULT hr = ::CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory));
            ASSERT_EQ(hr, S_OK);

            ComPtr<IDXGIAdapter> dxgiAdapter;
            hr = dxgiFactory->EnumAdapterByLuid(adapterLuid, IID_PPV_ARGS(&dxgiAdapter));
            ASSERT_EQ(hr, S_OK);

            ComPtr<ID3D11Device> d3d11Device;
            D3D_FEATURE_LEVEL d3dFeatureLevel;
            ComPtr<ID3D11DeviceContext> d3d11DeviceContext;
            hr = ::D3D11CreateDevice(dxgiAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
                                     nullptr, 0, D3D11_SDK_VERSION, &d3d11Device, &d3dFeatureLevel,
                                     &d3d11DeviceContext);
            ASSERT_EQ(hr, S_OK);

            mD3d11Device = std::move(d3d11Device);
        }

        ComPtr<ID3D11Device> mD3d11Device;
    };
}  // namespace

// Test creating a multi-planar video texture
TEST_P(VideoViewsTest, Create) {
    constexpr size_t kTextureWidth = 4;
    constexpr size_t kTextureHeight = 4;

    wgpu::TextureDescriptor textureDesc;
    textureDesc.format = wgpu::TextureFormat::NV12;
    textureDesc.dimension = wgpu::TextureDimension::e2D;
    textureDesc.usage = wgpu::TextureUsage::Sampled;
    textureDesc.size = {kTextureWidth, kTextureHeight, 1};

    D3D11_TEXTURE2D_DESC d3dDescriptor;
    d3dDescriptor.Width = kTextureWidth;
    d3dDescriptor.Height = kTextureHeight;
    d3dDescriptor.MipLevels = 1;
    d3dDescriptor.ArraySize = 1;
    d3dDescriptor.Format = DXGI_FORMAT_NV12;
    d3dDescriptor.SampleDesc.Count = 1;
    d3dDescriptor.SampleDesc.Quality = 0;
    d3dDescriptor.Usage = D3D11_USAGE_DEFAULT;
    d3dDescriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    d3dDescriptor.CPUAccessFlags = 0;
    d3dDescriptor.MiscFlags =
        D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    ComPtr<ID3D11Texture2D> d3d11Texture;
    HRESULT hr = mD3d11Device->CreateTexture2D(&d3dDescriptor, nullptr, &d3d11Texture);
    ASSERT_EQ(hr, S_OK);

    ComPtr<IDXGIResource1> dxgiResource;
    hr = d3d11Texture.As(&dxgiResource);
    ASSERT_EQ(hr, S_OK);

    HANDLE sharedHandle;
    hr = dxgiResource->CreateSharedHandle(
        nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &sharedHandle);
    ASSERT_EQ(hr, S_OK);

    dawn_native::d3d12::ExternalImageDescriptorDXGISharedHandle externDesc;
    externDesc.cTextureDescriptor = reinterpret_cast<const WGPUTextureDescriptor*>(&textureDesc);
    externDesc.sharedHandle = sharedHandle;
    externDesc.acquireMutexKey = 0;
    externDesc.isInitialized = true;

    wgpu::Texture texture =
        wgpu::Texture::Acquire(dawn_native::d3d12::WrapSharedHandle(device.Get(), &externDesc));

    ::CloseHandle(sharedHandle);

    // Luminance-only view
    wgpu::TextureViewDescriptor lumaView;
    lumaView.format = wgpu::TextureFormat::R8Unorm;
    lumaView.aspect = wgpu::TextureAspect::Plane0;
    wgpu::TextureView lumaTextureView = texture.CreateView(&lumaView);

    // Chrominance-only view
    wgpu::TextureViewDescriptor chromaView;
    chromaView.format = wgpu::TextureFormat::RG8Unorm;
    chromaView.aspect = wgpu::TextureAspect::Plane1;
    wgpu::TextureView chromaTextureView = texture.CreateView(&chromaView);

    // Create bind group
    wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture}});

    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);
    renderPipelineDescriptor.vertexStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
            #version 450
            layout(location=0) out vec2 o_texCoord;
            void main() {
                const vec2 pos[3] = vec2[3](vec2(-1.f, 1.f), 
                                            vec2(1.f, 1.f), 
                                            vec2(-1.f, -1.f));
        
                 const vec2 texCoord[3] = vec2[3](vec2(0.f, 0.f),
                                                  vec2(0.f, 1.f),
                                                  vec2(1.f, 0.f));
                gl_Position = vec4(pos[gl_VertexIndex], 0.f, 1.f);
                o_texCoord = texCoord[gl_VertexIndex];
            })");

    renderPipelineDescriptor.cFragmentStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
            #version 450
            layout (set = 0, binding = 0) uniform sampler sampler0;
            layout (set = 0, binding = 1) uniform texture2D lumaTexture;
            layout (set = 0, binding = 2) uniform texture2D chromaTexture;
            layout(location = 0) in vec2 texCoord;
            layout(location = 0) out vec4 fragColor;
            void main() {
               float y,u,v;
               y = texture(sampler2D(lumaTexture, sampler0), texCoord).r - 0.5;
               u = texture(sampler2D(chromaTexture, sampler0), texCoord).r - 0.5;
               v = texture(sampler2D(chromaTexture, sampler0), texCoord).g - 0.5;

               // YUV to RGB conversion constants
               float r,g,b;
               r = y + 1.13983*v;
               g = y - 0.39465*u - 0.58060*v;
               b = y + 2.03211*u;

               fragColor = vec4(r, g, b, 1.0);
            })");

    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);
    renderPipelineDescriptor.cColorStates[0].format = renderPass.colorFormat;

    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);

    wgpu::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(renderPipeline);
        pass.SetBindGroup(0, utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0),
                                                  {{0, sampler}, {1, lumaTextureView}, {2, chromaTextureView}}));
        pass.Draw(3);
        pass.EndPass();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);
}

DAWN_INSTANTIATE_TEST(VideoViewsTest, D3D12Backend());