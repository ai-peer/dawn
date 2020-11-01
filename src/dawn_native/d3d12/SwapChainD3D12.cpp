// Copyright 2017 The Dawn Authors
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

#include "dawn_native/d3d12/SwapChainD3D12.h"

#include "dawn_native/Surface.h"
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/TextureD3D12.h"

#include <dawn/dawn_wsi.h>

namespace dawn_native { namespace d3d12 {

    namespace {

        uint32_t BufferCountForPresentMode(wgpu::PresentMode mode) {
            switch (mode) {
                case wgpu::PresentMode::Immediate:
                    return 1;
                case wgpu::PresentMode::Fifo:
                    return 2;
                case wgpu::PresentMode::Mailbox:
                    return 3;
            }
        }

        uint32_t SwapIntervalForPresentMode(wgpu::PresentMode mode) {
            switch (mode) {
                case wgpu::PresentMode::Immediate:
                case wgpu::PresentMode::Mailbox:
                    return 0;
                case wgpu::PresentMode::Fifo:
                    return 1;
            }
        }

        UINT SwapChainFlagsForPresentMode(wgpu::PresentMode mode) {
            UINT flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

            if (mode == wgpu::PresentMode::Immediate) {
                flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
            }

            return flags;
        }

        DXGI_USAGE ToDXGIUsage(wgpu::TextureUsage usage) {
            DXGI_USAGE dxgiUsage = DXGI_CPU_ACCESS_NONE;
            if (usage & wgpu::TextureUsage::Sampled) {
                dxgiUsage |= DXGI_USAGE_SHADER_INPUT;
            }
            if (usage & wgpu::TextureUsage::Storage) {
                dxgiUsage |= DXGI_USAGE_UNORDERED_ACCESS;
            }
            if (usage & wgpu::TextureUsage::RenderAttachment) {
                dxgiUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;
            }
            return dxgiUsage;
        }

    }  // namespace

    // OldSwapChain

    OldSwapChain::OldSwapChain(Device* device, const SwapChainDescriptor* descriptor)
        : OldSwapChainBase(device, descriptor) {
        const auto& im = GetImplementation();
        DawnWSIContextD3D12 wsiContext = {};
        wsiContext.device = reinterpret_cast<WGPUDevice>(GetDevice());
        im.Init(im.userData, &wsiContext);

        ASSERT(im.textureUsage != WGPUTextureUsage_None);
        mTextureUsage = static_cast<wgpu::TextureUsage>(im.textureUsage);
    }

    OldSwapChain::~OldSwapChain() {
    }

    TextureBase* OldSwapChain::GetNextTextureImpl(const TextureDescriptor* descriptor) {
        const auto& im = GetImplementation();
        DawnSwapChainNextTexture next = {};
        DawnSwapChainError error = im.GetNextTexture(im.userData, &next);
        if (error) {
            GetDevice()->HandleError(InternalErrorType::Internal, error);
            return nullptr;
        }

        ComPtr<ID3D12Resource> d3d12Texture = static_cast<ID3D12Resource*>(next.texture.ptr);
        return new Texture(ToBackend(GetDevice()), descriptor, std::move(d3d12Texture));
    }

    MaybeError OldSwapChain::OnBeforePresent(TextureViewBase* view) {
        Device* device = ToBackend(GetDevice());

        CommandRecordingContext* commandContext;
        DAWN_TRY_ASSIGN(commandContext, device->GetPendingCommandContext());

        // Perform the necessary transition for the texture to be presented.
        ToBackend(view->GetTexture())
            ->TrackUsageAndTransitionNow(commandContext, mTextureUsage,
                                         view->GetSubresourceRange());

        DAWN_TRY(device->ExecutePendingCommandContext());

        return {};
    }

    // SwapChain

    // static
    ResultOrError<SwapChain*> SwapChain::Create(Device* device,
                                                Surface* surface,
                                                NewSwapChainBase* previousSwapChain,
                                                const SwapChainDescriptor* descriptor) {
        std::unique_ptr<SwapChain> swapchain =
            std::make_unique<SwapChain>(device, surface, descriptor);
        DAWN_TRY(swapchain->Initialize(previousSwapChain));
        return swapchain.release();
    }

    SwapChain::~SwapChain() {
        DetachFromSurface();
    }

    MaybeError SwapChain::Initialize(NewSwapChainBase* previousSwapChain) {
        ASSERT(GetSurface()->GetType() == Surface::Type::WindowsHWND);

        Device* device = ToBackend(GetDevice());

        uint32_t bufferCount = BufferCountForPresentMode(GetPresentMode());
        DXGI_FORMAT format = D3D12TextureFormat(GetFormat());
        UINT swapChainFlags = SwapChainFlagsForPresentMode(GetPresentMode());

        if (previousSwapChain != nullptr) {
            // TODO(cwallez@chromium.org): figure out what should happen when surfaces are used by
            // multiple backends one after the other. It probably needs to block until the backend
            // and GPU are completely finished with the previous swapchain.
            if (previousSwapChain->GetBackendType() != wgpu::BackendType::D3D12) {
                return DAWN_VALIDATION_ERROR("d3d12::SwapChain cannot switch between APIs");
            }

            // TODO(cwallez@chromium.org): use ToBackend once OldSwapChainBase is removed.
            SwapChain* previousD3D12SwapChain = static_cast<SwapChain*>(previousSwapChain);

            // TODO(cwallez@chromium.org): Figure out switching an HWND between devices, it might
            // require just losing the reference to the swapchain, but might also need to wait for
            // all previous operations to complete.
            if (GetDevice() != previousSwapChain->GetDevice()) {
                return DAWN_VALIDATION_ERROR("d3d12::SwapChain cannot switch between devices");
            }

            // The preivous swapchain is on the same device so we can reuse it and its buffers
            // directly but lose a cess to them
            std::swap(previousD3D12SwapChain->mDXGISwapChain, mDXGISwapChain);

            // If the swapchains are similar enough we can reuse the content of the previous
            // swapchain directly and be done.
            bool canReuseBuffers = GetWidth() == previousSwapChain->GetWidth() &&
                                   GetHeight() == previousSwapChain->GetHeight() &&
                                   GetFormat() == previousSwapChain->GetFormat() &&
                                   GetPresentMode() == previousSwapChain->GetPresentMode();
            if (canReuseBuffers) {
                std::swap(mBuffers, previousD3D12SwapChain->mBuffers);
                std::swap(mBufferSerials, previousD3D12SwapChain->mBufferSerials);
                mCurrentBuffer = previousD3D12SwapChain->mCurrentBuffer;

                return {};
            }

            // We need to resize, IDXGSwapChain->ResizeBuffers requires that all references to
            // buffers are lost before it is called. These references are in the previous
            // swapchains' mBuffers but also in its current texture if any. Just detach the previous
            // swapchain so that everything is cleared.
            previousD3D12SwapChain->DetachFromSurface();

            mDXGISwapChain->ResizeBuffers(bufferCount, GetWidth(), GetHeight(), format,
                                          swapChainFlags);
        }

        // If we haven't been able to reuse the DXGI swapchain, create a new one.
        if (mDXGISwapChain == nullptr) {
            DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
            swapChainDesc.Width = GetWidth();
            swapChainDesc.Height = GetHeight();
            swapChainDesc.Format = format;
            swapChainDesc.Stereo = false;
            swapChainDesc.SampleDesc.Count = 1;
            swapChainDesc.SampleDesc.Quality = 0;
            swapChainDesc.BufferUsage = ToDXGIUsage(GetUsage());
            swapChainDesc.BufferCount = bufferCount;
            swapChainDesc.Scaling = DXGI_SCALING_NONE;
            swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            swapChainDesc.Flags = swapChainFlags;

            IDXGIFactory2* factory2 = nullptr;
            DAWN_TRY(CheckHRESULT(device->GetFactory()->QueryInterface(IID_PPV_ARGS(&factory2)),
                                  "Getting IDXGIFactory2"));

            ComPtr<IDXGISwapChain1> swapChain1;
            factory2->CreateSwapChainForHwnd(device->GetCommandQueue().Get(),
                                             static_cast<HWND>(GetSurface()->GetHWND()),
                                             &swapChainDesc, nullptr, nullptr, &swapChain1);
            DAWN_TRY(CheckHRESULT(swapChain1.As(&mDXGISwapChain), ""));
        }

        // Gather the buffers from the new DXGISwapChain or from its resize.
        ASSERT(mBuffers.empty());
        mBuffers.resize(bufferCount);
        for (uint32_t i = 0; i < bufferCount; i++) {
            DAWN_TRY(CheckHRESULT(mDXGISwapChain->GetBuffer(i, IID_PPV_ARGS(&mBuffers[i])),
                                  "Getting IDXGISwapChain buffer"));
        }

        mBufferSerials.resize(bufferCount, ExecutionSerial(0));

        return {};
    }

    MaybeError SwapChain::PresentImpl() {
        Device* device = ToBackend(GetDevice());

        CommandRecordingContext* commandContext;
        DAWN_TRY_ASSIGN(commandContext, device->GetPendingCommandContext());

        // TODO(cwallez@chromium.org): XXX write the comment
        mApiTexture->TrackUsageAndTransitionNow(commandContext, kPresentTextureUsage,
                                                mApiTexture->GetAllSubresources());

        DAWN_TRY(device->ExecutePendingCommandContext());

        DAWN_TRY(
            CheckHRESULT(mDXGISwapChain->Present(SwapIntervalForPresentMode(GetPresentMode()), 0),
                         "IDXGISwapChain::Present"));

        // XXX WTF?
        DAWN_TRY(device->NextSerial());
        mBufferSerials[mCurrentBuffer] = device->GetPendingCommandSerial();

        mApiTexture->Destroy();
        mApiTexture = nullptr;

        return {};
    }

    ResultOrError<TextureViewBase*> SwapChain::GetCurrentTextureViewImpl() {
        Device* device = ToBackend(GetDevice());

        mCurrentBuffer = mDXGISwapChain->GetCurrentBackBufferIndex();

        TextureDescriptor descriptor = GetSwapChainBaseTextureDescriptor(this);
        mApiTexture = new Texture(ToBackend(GetDevice()), &descriptor, mBuffers[mCurrentBuffer]);

        // XXX GPU Wait?
        DAWN_TRY(device->WaitForSerial(mBufferSerials[mCurrentBuffer]));

        return mApiTexture->CreateView(nullptr);
    }

    void SwapChain::DetachFromSurfaceImpl() {
        if (mApiTexture) {
            mApiTexture->Destroy();
            mApiTexture = nullptr;
        }

        mDXGISwapChain = nullptr;
        mBuffers.clear();
    }

}}  // namespace dawn_native::d3d12
