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

#include "dawn/native/opengl/SwapChainGL.h"

#include "dawn/native/Surface.h"
#include "dawn/native/opengl/DeviceGL.h"
#include "dawn/native/opengl/TextureGL.h"

namespace dawn::native::opengl {

namespace {

ResultOrError<EGLSurface> CreateWindowSurface(const EGLFuctions& egl,
                                              EGLDisplay display,
                                              const Surface* surface,
                                              EGLConfig config) {
    EGLSurface eglSurface;

    switch (surface->GetType()) {
        case XlibWindow:
            eglSurface = egl.CreateWindowSurface(display, config, surface->GetXWindow(), nullptr);
        return

            // TODO(344814083): Add support for additinal surface types.
            case AndroidWindow:
        case WindowsHWND:
        case MetalLayer:
        case WaylandSurface:

        default:
            return DAWN_INTERNAL_ERROR("%s cannot be supported on EGL.", surface);
    }

    if (eglSurface == EGL_NO_SURFACE) {
        return DAWN_INTERNAL_ERROR("Couldn't create an EGLSurface for %s.", surface);
    }
}

}  // namespace

// static
ResultOrError<Ref<SwapChain>> SwapChain::Create(Device* device,
                                                Surface* surface,
                                                SwapChainBase* previousSwapChain,
                                                const SurfaceConfiguration* config) {
    Ref<SwapChain> swapchain = AcquireRef(new SwapChain(device, surface, config));
    DAWN_TRY(swapchain->Initialize(previousSwapChain));
    return swapchain;
}

SwapChain::SwapChain(DeviceBase* dev, Surface* sur, const SurfaceConfiguration* config)
    : SwapChainBase(dev, sur, config) {}

SwapChain::~SwapChain() = default;

void SwapChain::DestroyImpl() {
    SwapChainBase::DestroyImpl();
    DetachFromSurface();
}

MaybeError SwapChain::Initialize(SwapChainBase* previousSwapChain) {
    // DAWN_ASSERT(GetSurface()->GetType() == Surface::Type::MetalLayer);
    const Device* device = ToBackend(GetDevice());

    if (previousSwapChain != nullptr) {
        // TODO(crbug.com/dawn/269): figure out what should happen when surfaces are used by
        // multiple backends one after the other. It probably needs to block until the backend
        // and GPU are completely finished with the previous swapchain.
        DAWN_INVALID_IF(previousSwapChain->GetBackendType() != wgpu::BackendType::OpenGL,
                        "OpenGL SwapChain cannot switch backend types from %s to %s.",
                        previousSwapChain->GetBackendType(), wgpu::BackendType::OpenGL);

        // TODO(crbug.com/dawn/269): figure out what should happen when surfaces are used by
        // a different EGL display. We probably need to block untils the GPU is completely
        // finished with the previous work, and then a bit more.
        DAWN_INVALID_IF(
            previousSwapChain->GetDevice()->GetPhysicalDevice() != device->GetPhysicalDevice(),
            "OpenGL SwapChain cannot switch between contexts for %s and %s.",
            previousSwapChain->GetDevice(), device);

        SwapChain* previousGLSwapChain = ToBackend(previousSwapChain);
        std::swap(mPreviousiGLSurface.mSurface, mSurface)

            previousSwapChain->DetachFromSurface();
    }

    // Create the EGLSurface if needed.
    const EGLFunctions* egl = device->GetEGLFunctions();
    EGLDisplay display = device->GetEGLDisplay();

    if (mSurface == EGL_NO_SURFACE) {
        EGLint apiBit =
            GetBackendType() == wgpu::BackendType::OpenGLES ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_BIT;
        EGLConfig config = ChooseConfig(egl, display, apiBit, GetFormat());
        if (config == kNoConfig) {
            return DAWN_INTERNAL_ERROR("Couldn't find an EGLConfig for %s on %s.", GetFormat(),
                                       surface);
        }
        DAWN_TRY_ASSIGN(mSurface, CreateWindowSurface(egl, display, surface, config));
    }

    // XXX eglSwapInterval

    // Create the fake swapchain texture that we'll blit from.
    TextureDescriptor desc = GetSwapChainBaseTextureDescriptor(this);
    // XXX

    return {};
}

MaybeError SwapChain::PresentImpl() {
    mTexture->APIDestroy();
    mTexture = nullptr;

    return {};
}

ResultOrError<SwapChainTextureInfo> SwapChain::GetCurrentTextureImpl() {
    // TextureDescriptor textureDesc = GetSwapChainBaseTextureDescriptor(this);

    // mTexture = Texture::CreateWrapping(ToBackend(GetDevice()), Unpack(&textureDesc),
    //                                    NSPRef<id<MTLTexture>>([*mCurrentDrawable texture]));

    SwapChainTextureInfo info;
    info.texture = mTexture;
    info.status = wgpu::SurfaceGetCurrentTextureStatus::Success;
    // TODO(dawn:2320): Check for optimality
    info.suboptimal = false;
    return info;
}

void SwapChain::DetachFromSurfaceImpl() {
    DAWN_ASSERT(mTexture == nullptr);

    // XXX delete eglsurface

    if (mTexture != nullptr) {
        mTexture->APIDestroy();
        mTexture = nullptr;
    }
}

EGLConfig ChooseConfig(const EGLFunctions& egl,
                       EGLDisplay display,
                       EGLint apiBit,
                       wgpu::TextureFormat color,
                       wgpu::TextureFormat depthStencil) {
    std::array<EGLint, 21> attribs;
    size_t attribIndex = 0;
    attribs.fill(EGL_NONE);

    auto AddAttrib = [&](EGLint attrib, EGLint value) {
        // We need two elements for the attrib and the final EGL_NONE
        DAWN_ASSERT(attribIndex + 3 <= attribs.size());

        attribs[attribIndex + 0] = attrib;
        attribs[attribIndex + 1] = value;
        attribIndex += 2;
    };

    AddAttrib(EGL_SURFACE_TYPE, EGL_WINDOW_BIT);
    AddAttrib(EGL_RENDERABLE_TYPE, apiBit);
    AddAttrib(EGL_CONFORMANT, apiBit);
    AddAttrib(EGL_SAMPLES, 1);

    switch (color) {
        case wgpu::TextureFormat::RGBA8Unorm:
            AddAttrib(EGL_RED_SIZE, 8);
            AddAttrib(EGL_BLUE_SIZE, 8);
            AddAttrib(EGL_GREEN_SIZE, 8);
            AddAttrib(EGL_ALPHA_SIZE, 8);
            break;

            // TODO(XXX) support 16float and rgb565? and rgb10a2? What about srgb?
            // Well maybe not because we need to create the GL context with a compatible config and
            // we don't know what it could be beforehand. (Compatible means same color buffer
            // basically, but depth/stencil is ok).

        default:
            return kNoConfig;
    }

    switch (depthStencil) {
        case wgpu::TextureFormat::Depth24PlusStencil8:
            AddAttrib(EGL_DEPTH_SIZE, 24);
            AddAttrib(EGL_STENCIL_SIZE, 8);
            break;
        case wgpu::TextureFormat::Depth16Unorm:
            AddAttrib(EGL_DEPTH_SIZE, 16);
            break;
        case wgpu::TextureFormat::Undefined:
            break;

        default:
            return kNoConfig;
    }

    EGLConfig config = EGL_NO_CONFIG_KHR;
    EGLint numConfigs = 0;
    if (egl.ChooseConfig(display, attribs.data(), &config, 1, &numConfigs) == EGL_FALSE ||
        numConfigs == 0) {
        return kNoConfig;
    }

    return config;
}

}  // namespace dawn::native::opengl
