// Copyright 2022 The Dawn & Tint Authors
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

#include "dawn/native/opengl/ContextEGL.h"

#include <memory>
#include <string>
#include <vector>

#include "dawn/native/opengl/UtilsEGL.h"

#ifndef EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE
#define EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE 0x33AF
#endif

namespace dawn::native::opengl {

// static
ResultOrError<std::unique_ptr<ContextEGL>> CreateFromDynamicLoading(wgpu::BackendType backend, std::string_view libName) {
    auto context = std::make_unique<ContextEGL>(backend);
    context->InitializeWithDynamicLoading(libName);
    return std::move(context);
}

// static
ResultOrError<std::unique_ptr<ContextEGL>> CreateFromProcAndDisplay(wgpu::BackendType backend, EGLGetProcProc getProc, EGLDisplay display) {
    auto context = std::make_unique<ContextEGL>(backend);
    context->InitializeWithProcAndDisplay(libName);
    return std::move(context);
}

ResultOrError<std::unique_ptr<ContextEGL>> ContextEGL::Create(const EGLFunctions& egl,
                                                              EGLDisplay display,
                                                              wgpu::BackendType backend,
                                                              bool useANGLETextureSharing) {
    auto context = std::make_unique<ContextEGL>(egl, display, backend);
    DAWN_TRY(context->Initialize(useANGLETextureSharing));
    return std::move(context);
}

ContextEGL::ContextEGL(const EGLFunctions& functions, EGLDisplay display, wgpu::BackendType backend)
    : mEgl(functions), mDisplay(display) {
    switch (backend) {
        case wgpu::BackendType::OpenGL:
            mApiEnum = EGL_OPENGL_API;
            mApiBit = EGL_OPENGL_ES3_BIT;
            break;
        case wgpu::BackendType::OpenGLES:
            mApiEnum = EGL_OPENGL_ES_API;
            mApiBit = EGL_OPENGL_BIT;
            break;
        default:
            DAWN_UNREACHABLE();
    }
}

ContextEGL::~ContextEGL() {
    mEgl.DestroyContext(mDisplay, mContext);
}

MaybeError ContextEGL::Initialize(bool useANGLETextureSharing) {
    // We require at least EGL 1.4.
    DAWN_INVALID_IF(
        mEgl.GetMajorVersion() < 1 || (mEgl.GetMajorVersion() == 1 && mEgl.GetMinorVersion() < 4),
        "EGL version (%u.%u) must be at least 1.4", mEgl.GetMajorVersion(), mEgl.GetMinorVersion());

    // Since we're creating a surfaceless context, the only thing we really care
    // about is the RENDERABLE_TYPE.
    EGLint config_attribs[] = {EGL_RENDERABLE_TYPE, mApiBit, EGL_NONE};

    EGLint numConfig;
    EGLConfig config;
    DAWN_TRY(CheckEGL(mEgl, mEgl.ChooseConfig(mDisplay, config_attribs, &config, 1, &numConfig),
                      "eglChooseConfig"));

    DAWN_INVALID_IF(numConfig == 0, "eglChooseConfig returned zero configs");

    DAWN_TRY(CheckEGL(mEgl, mEgl.BindAPI(mApiEnum), "eglBindAPI"));

    if (!mEgl.HasExt(EGLExt::ImageBase)) {
        return DAWN_INTERNAL_ERROR("EGL_KHR_image_base is required.");
    }
    if (!mEgl.HasExt(EGLExt::CreateContextRobustness)) {
        return DAWN_INTERNAL_ERROR("EGL_EXT_create_context_robustness is required.");
    }

    if (!mEgl.HasExt(EGLExt::FenceSync) && !mEgl.HasExt(EGLExt::ReusableSync)) {
        return DAWN_INTERNAL_ERROR("EGL_KHR_fence_sync or EGL_KHR_reusable_sync must be supported");
    }

    int major, minor;
    if (mApiEnum == EGL_OPENGL_ES_API) {
        major = 3;
        minor = 1;
    } else {
        major = 4;
        minor = 4;
    }
    std::vector<EGLint> attribs{
        EGL_CONTEXT_MAJOR_VERSION,
        major,
        EGL_CONTEXT_MINOR_VERSION,
        minor,
        EGL_CONTEXT_OPENGL_ROBUST_ACCESS,  // Core in EGL 1.5
        EGL_TRUE,
    };
    if (useANGLETextureSharing) {
        if (!mEgl.HasExt(EGLExt::DisplayTextureShareGroup)) {
            return DAWN_INTERNAL_ERROR(
                "EGL_GL_ANGLE_display_texture_share_group must be supported to use GL texture "
                "sharing");
        }
        attribs.push_back(EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE);
        attribs.push_back(EGL_TRUE);
    }
    attribs.push_back(EGL_NONE);

    mContext = mEgl.CreateContext(mDisplay, config, EGL_NO_CONTEXT, attribs.data());
    return CheckEGL(mEgl, mContext != EGL_NO_CONTEXT, "eglCreateContext");
}

void ContextEGL::MakeCurrent() {
    EGLBoolean success = mEgl.MakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, mContext);
    DAWN_ASSERT(success == EGL_TRUE);
}

EGLDisplay ContextEGL::GetEGLDisplay() const {
    return mDisplay;
}

const EGLFunctions& ContextEGL::GetEGL() const {
    return mEgl;
}

EGLint ContextEGL::GetAPIEnum() const {
    return mApiEnum;
}
EGLint ContextEGL::GetAPIBit() const {
    return mApiBit;
}

}  // namespace dawn::native::opengl
