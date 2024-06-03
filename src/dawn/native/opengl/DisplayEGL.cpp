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

#include "dawn/native/opengl/DisplayEGL.h"

#include <string>
#include <utility>

namespace dawn::native::opengl {

// static
ResultOrError<std::unique_ptr<DisplayEGL>> DisplayEGL::CreateFromDynamicLoading(
    wgpu::BackendType backend,
    const char* libName) {
    auto display = std::make_unique<DisplayEGL>(backend);
    DAWN_TRY(display->InitializeWithDynamicLoading(libName));
    return std::move(display);
}

// static
ResultOrError<std::unique_ptr<DisplayEGL>> DisplayEGL::CreateFromProcAndDisplay(
    wgpu::BackendType backend,
    EGLGetProcProc getProc,
    EGLDisplay eglDisplay) {
    auto display = std::make_unique<DisplayEGL>(backend);
    DAWN_TRY(display->InitializeWithProcAndDisplay(getProc, eglDisplay));
    return std::move(display);
}

DisplayEGL::DisplayEGL(wgpu::BackendType backend) : egl(mFunctions) {
    switch (backend) {
        case wgpu::BackendType::OpenGL:
            mApiEnum = EGL_OPENGL_API;
            mApiBit = EGL_OPENGL_BIT;
            break;
        case wgpu::BackendType::OpenGLES:
            mApiEnum = EGL_OPENGL_ES_API;
            mApiBit = EGL_OPENGL_ES3_BIT;
            break;
        default:
            DAWN_UNREACHABLE();
    }
}

DisplayEGL::~DisplayEGL() {
    if (mDisplay != EGL_NO_DISPLAY) {
        if (mOwnsDisplay) {
            egl.Terminate(mDisplay);
        }
        mDisplay = EGL_NO_DISPLAY;
    }
}

MaybeError DisplayEGL::InitializeWithDynamicLoading(const char* libName) {
    std::string err;
    if (!mLib.Valid() && !mLib.Open(libName, &err)) {
        return DAWN_VALIDATION_ERROR("Failed to load %s: \"%s\".", libName, err.c_str());
    }

    EGLGetProcProc getProc = reinterpret_cast<EGLGetProcProc>(mLib.GetProc("eglGetProcAddress"));
    if (!getProc) {
        return DAWN_VALIDATION_ERROR("Couldn't get \"eglGetProcAddress\" from %s.", libName);
    }

    return InitializeWithProcAndDisplay(getProc, EGL_NO_DISPLAY);
}

MaybeError DisplayEGL::InitializeWithProcAndDisplay(EGLGetProcProc getProc, EGLDisplay display) {
    // Load the EGL functions.
    DAWN_TRY(mFunctions.LoadClientProcs(getProc));

    mDisplay = display;
    if (mDisplay == EGL_NO_DISPLAY) {
        mOwnsDisplay = true;
        mDisplay = egl.GetDisplay(EGL_DEFAULT_DISPLAY);
    }
    if (mDisplay == EGL_NO_DISPLAY) {
        return DAWN_VALIDATION_ERROR("Couldn't create the default EGL display.");
    }

    DAWN_TRY(mFunctions.LoadDisplayProcs(mDisplay));

    // We require at least EGL 1.4.
    DAWN_INVALID_IF(
        egl.GetMajorVersion() < 1 || (egl.GetMajorVersion() == 1 && egl.GetMinorVersion() < 4),
        "EGL version (%u.%u) must be at least 1.4", egl.GetMajorVersion(), egl.GetMinorVersion());

    return {};
}

EGLDisplay DisplayEGL::GetDisplay() const {
    return mDisplay;
}

EGLint DisplayEGL::GetAPIEnum() const {
    return mApiEnum;
}

EGLint DisplayEGL::GetAPIBit() const {
    return mApiBit;
}

EGLConfig DisplayEGL::ChooseConfig(EGLint surfaceType,
                                   wgpu::TextureFormat color,
                                   wgpu::TextureFormat depthStencil) const {
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

    AddAttrib(EGL_SURFACE_TYPE, surfaceType);
    // XXX ????
    // AddAttrib(EGL_RENDERABLE_TYPE, apiBit);
    // AddAttrib(EGL_CONFORMANT, apiBit);
    // AddAttrib(EGL_SAMPLES, 1);

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
    if (egl.ChooseConfig(mDisplay, attribs.data(), &config, 1, &numConfigs) == EGL_FALSE ||
        numConfigs == 0) {
        return kNoConfig;
    }

    return config;
}

}  // namespace dawn::native::opengl
