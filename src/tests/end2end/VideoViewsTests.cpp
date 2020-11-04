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

// Re-gen via `python bmp_to_nv12.py webgpu.bmp webgpu_nv12.h webgpu`
#include "webgpu_nv12.inc"

using Microsoft::WRL::ComPtr;

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

// Renders using a DX11 NV12 texture.
// Samples a YUV quad then reads back the RGB values to ensure they are correct.
TEST_P(VideoViewsTest, NV12) {
    wgpu::TextureDescriptor textureDesc;
    textureDesc.format = wgpu::TextureFormat::NV12;
    textureDesc.dimension = wgpu::TextureDimension::e2D;
    textureDesc.usage = wgpu::TextureUsage::Sampled;
    textureDesc.size = {webgpu_width, webgpu_height, 1};

    D3D11_TEXTURE2D_DESC d3dDescriptor;
    d3dDescriptor.Width = webgpu_width;
    d3dDescriptor.Height = webgpu_height;
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

    D3D11_SUBRESOURCE_DATA subres;
    subres.pSysMem = webgpu_data;
    subres.SysMemPitch = webgpu_width;
    subres.SysMemSlicePitch = webgpu_width * webgpu_height * 3 / 2;

    ComPtr<ID3D11Texture2D> d3d11Texture;
    HRESULT hr = mD3d11Device->CreateTexture2D(&d3dDescriptor, &subres, &d3d11Texture);
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
    externDesc.acquireMutexKey = 1;
    externDesc.isInitialized = true;

    // DX11 texture should be initialized upon CreateTexture2D. However, if we do not
    // acquire/release the keyed mutex before using the wrapped WebGPU texture, the WebGPU texture
    // is left uninitialized.
    ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex;
    hr = d3d11Texture.As(&dxgiKeyedMutex);
    ASSERT_EQ(hr, S_OK);

    hr = dxgiKeyedMutex->AcquireSync(0, INFINITE);
    ASSERT_EQ(hr, S_OK);

    hr = dxgiKeyedMutex->ReleaseSync(1);
    ASSERT_EQ(hr, S_OK);

    wgpu::Texture wgpuTexture =
        wgpu::Texture::Acquire(dawn_native::d3d12::WrapSharedHandle(device.Get(), &externDesc));

    ::CloseHandle(sharedHandle);

    // Luminance-only view
    wgpu::TextureViewDescriptor lumaView;
    lumaView.format = wgpu::TextureFormat::R8Unorm;
    lumaView.aspect = wgpu::TextureAspect::Plane0;
    wgpu::TextureView lumaTextureView = wgpuTexture.CreateView(&lumaView);

    // Chrominance-only view
    wgpu::TextureViewDescriptor chromaView;
    chromaView.format = wgpu::TextureFormat::RG8Unorm;
    chromaView.aspect = wgpu::TextureAspect::Plane1;
    wgpu::TextureView chromaTextureView = wgpuTexture.CreateView(&chromaView);

    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);
    renderPipelineDescriptor.vertexStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
            #version 450
            layout(location=0) out vec2 o_texCoord;
            void main() {
                const vec2 pos[6] = vec2[6](vec2(-1.f, 1.f), 
                                            vec2(-1.f, -1.f), 
                                            vec2(1.f, -1.f),
                                            vec2(-1.f, 1.f),
                                            vec2(1.f, -1.f),
                                            vec2(1.f, 1.f));

                gl_Position = vec4(pos[gl_VertexIndex], 0.f, 1.f);
                o_texCoord = (gl_Position.xy * 0.5) + 0.5;
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
               y = texture(sampler2D(lumaTexture, sampler0), texCoord).r - (16.0 / 256.0);
               u = texture(sampler2D(chromaTexture, sampler0), texCoord).r - 0.5;
               v = texture(sampler2D(chromaTexture, sampler0), texCoord).g - 0.5;

               // YUV to RGB conversion
               float r,g,b;
               r = y + 1.164383 * y + 1.596027 * v;
               g = 1.164383 * y - 0.391762 * u - 0.812968 * v;
               b = 1.164383 * y + 2.017232 * u;

               fragColor = vec4(r, g, b, 1.0);
            })");

    utils::BasicRenderPass renderPass =
        utils::CreateBasicRenderPass(device, webgpu_width, webgpu_height);
    renderPipelineDescriptor.cColorStates[0].format = renderPass.colorFormat;
    renderPipelineDescriptor.primitiveTopology = wgpu::PrimitiveTopology::TriangleList;

    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);

    wgpu::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(renderPipeline);
        pass.SetBindGroup(
            0, utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0),
                                    {{0, sampler}, {1, lumaTextureView}, {2, chromaTextureView}}));
        pass.Draw(6);
        pass.EndPass();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    RGBA8 expectedPixel(0x01, 0x00, 0x01, 0xFF);
    EXPECT_PIXEL_RGBA8_EQ(expectedPixel, renderPass.color, 0, 0);
}

DAWN_INSTANTIATE_TEST(VideoViewsTest, D3D12Backend());