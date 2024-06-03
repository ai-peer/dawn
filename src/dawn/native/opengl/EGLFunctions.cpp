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

#include "dawn/native/opengl/EGLFunctions.h"

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_split.h"
#include "dawn/native/opengl/UtilsEGL.h"

namespace dawn::native::opengl {

namespace {

using VersionPromoted = std::tuple<uint32_t, uint32_t>;
constexpr VersionPromoted EGL1_5 = {1, 5};
constexpr VersionPromoted NeverPromoted = {100000, 0};

struct ExtensionInfo {
    EGLExt index;
    const char* name;
    VersionPromoted versionPromoted;
};

static constexpr size_t kExtensionCount = static_cast<size_t>(EGLExt::EnumCount);
static constexpr std::array<ExtensionInfo, kExtensionCount> kExtensionInfos{{
    //
    {EGLExt::ClientExtensions, "EGL_EXT_client_extensions", EGL1_5},
    {EGLExt::PlatformBase, "EGL_EXT_platform_base", EGL1_5},
    {EGLExt::FenceSync, "EGL_KHR_fence_sync", EGL1_5},
    {EGLExt::CLEvent2, "EGL_KHR_cl_event2", EGL1_5},
    {EGLExt::WaitSync, "EGL_KHR_wait_sync", EGL1_5},
    {EGLExt::ImageBase, "EGL_KHR_image_base", EGL1_5},
    {EGLExt::GLTexture2DImage, "EGL_KHR_gl_texture_2D_image", EGL1_5},
    {EGLExt::GLTexture3DImage, "EGL_KHR_gl_texture_3D_image", EGL1_5},
    {EGLExt::GLTextureCubemapImage, "EGL_KHR_gl_texture_cubemap_image", EGL1_5},
    {EGLExt::GLRenderBufferImage, "EGL_KHR_gl_renderbuffer_image", EGL1_5},
    {EGLExt::CreateContext, "EGL_KHR_create_context", EGL1_5},
    {EGLExt::CreateContextRobustness, "EGL_EXT_create_context_robustness", EGL1_5},
    {EGLExt::GetAllProcAddresses, "EGL_KHR_get_all_proc_addresses", EGL1_5},
    {EGLExt::ClientGetAllProcAddresses, "EGL_KHR_client_get_all_proc_addresses", EGL1_5},
    {EGLExt::GLColorSpace, "EGL_KHR_gl_colorspace", EGL1_5},
    {EGLExt::SurfacelessContext, "EGL_KHR_surfaceless_context", EGL1_5},

    {EGLExt::DisplayTextureShareGroup, "EGL_ANGLE_display_texture_share_group", NeverPromoted},
    {EGLExt::ReusableSync, "EGL_KHR_reusable_sync", NeverPromoted},
    //
}};

}  // anonymous namespace

#define GET_PROC_WITH_NAME(member, name)                                          \
    do {                                                                          \
        member = reinterpret_cast<decltype(member)>(GetProcAddress(name));        \
        if (member == nullptr) {                                                  \
            return DAWN_INTERNAL_ERROR(std::string("Couldn't get proc ") + name); \
        }                                                                         \
    } while (0)

#define GET_PROC(name) GET_PROC_WITH_NAME(name, "egl" #name)

MaybeError EGLFunctions::Init(EGLDisplay display, void* (*getProc)(const char*)) {
    // Load EGL 1.0
    GetProcAddress = reinterpret_cast<decltype(GetProcAddress)>(getProc);

    GET_PROC(ChooseConfig);
    GET_PROC(CopyBuffers);
    GET_PROC(CreateContext);
    GET_PROC(CreatePbufferSurface);
    GET_PROC(CreatePixmapSurface);
    GET_PROC(CreateWindowSurface);
    GET_PROC(DestroyContext);
    GET_PROC(DestroySurface);
    GET_PROC(GetConfigAttrib);
    GET_PROC(GetConfigs);
    GET_PROC(GetCurrentDisplay);
    GET_PROC(GetCurrentSurface);
    GET_PROC(GetDisplay);
    GET_PROC(GetError);
    GET_PROC(Initialize);
    GET_PROC(MakeCurrent);
    GET_PROC(QueryContext);
    GET_PROC(QueryString);
    GET_PROC(QuerySurface);
    GET_PROC(SwapBuffers);
    GET_PROC(Terminate);
    GET_PROC(WaitGL);
    GET_PROC(WaitNative);

    // Get the EGL version and extensions.
    {
        EGLint major, minor;
        DAWN_TRY(CheckEGL(*this, Initialize(display, &major, &minor), "eglInitialize"));
        DAWN_INVALID_IF(major != 1, "EGL version (%u) is not 1.", major);
        mMajorVersion = major;
        mMinorVersion = minor;
    }

    {
        const char* rawExtensions = QueryString(display, EGL_EXTENSIONS);
        absl::flat_hash_set<std::string_view> extensions = absl::StrSplit(rawExtensions, " ");

        for (const ExtensionInfo& ext : kExtensionInfos) {
            if (std::tie(mMajorVersion, mMinorVersion) >= ext.versionPromoted ||
                extensions.contains(ext.name)) {
                mExtensions.set(ext.index);
            }
        }
    }

    // EGL 1.1
    if (mMinorVersion >= 1) {
        GET_PROC(BindTexImage);
        GET_PROC(ReleaseTexImage);
        GET_PROC(SurfaceAttrib);
        GET_PROC(SwapInterval);
    }

    // EGL 1.2
    if (mMinorVersion >= 2) {
        GET_PROC(BindAPI);
        GET_PROC(QueryAPI);
        GET_PROC(CreatePbufferFromClientBuffer);
        GET_PROC(ReleaseThread);
        GET_PROC(WaitClient);
    }

    // EGL 1.3 (no new procs)

    // EGL 1.4
    if (mMinorVersion >= 4) {
        GET_PROC(GetCurrentContext);
    }

    // EGL 1.5
    if (mMinorVersion >= 5) {
        GET_PROC(CreateSync);
        GET_PROC(DestroySync);
        GET_PROC(ClientWaitSync);
        GET_PROC(GetSyncAttrib);
        GET_PROC(CreateImage);
        GET_PROC(DestroyImage);
        GET_PROC(GetPlatformDisplay);
        GET_PROC(CreatePlatformWindowSurface);
        GET_PROC(CreatePlatformPixmapSurface);
        GET_PROC(WaitSync);
    } else {
        // Load extensions that would otherwise be promoted to EGL 1.5.

        if (HasExt(EGLExt::FenceSync)) {
            GET_PROC_WITH_NAME(ClientWaitSync, "eglClientWaitSyncKHR");
            GET_PROC_WITH_NAME(CreateSync, "eglCreateSyncKHR");
            GET_PROC_WITH_NAME(DestroySync, "eglDestroySyncKHR");
            GET_PROC_WITH_NAME(ClientWaitSync, "eglClientWaitSyncKHR");
        }

        if (HasExt(EGLExt::PlatformBase)) {
            GET_PROC_WITH_NAME(GetPlatformDisplay, "eglGetPlatformDisplayEXT");
            GET_PROC_WITH_NAME(CreatePlatformWindowSurface, "eglCreatePlatformWindowSurfaceEXT");
            GET_PROC_WITH_NAME(CreatePlatformPixmapSurface, "eglCreatePlatformPixmapSurfaceEXT");
        }

        if (HasExt(EGLExt::ImageBase)) {
            GET_PROC_WITH_NAME(CreateImage, "eglCreateImageKHR");
            GET_PROC_WITH_NAME(DestroyImage, "eglDestroyImageKHR");
        }

        if (HasExt(EGLExt::WaitSync)) {
            GET_PROC_WITH_NAME(WaitSync, "eglWaitSyncKHR");
        }
    }

    // Other extensions

    if (HasExt(EGLExt::ReusableSync)) {
        GET_PROC_WITH_NAME(SignalSync, "eglSignalSyncKHR");
    }

    return {};
}

uint32_t EGLFunctions::GetMajorVersion() const {
    return mMajorVersion;
}

uint32_t EGLFunctions::GetMinorVersion() const {
    return mMinorVersion;
}

bool EGLFunctions::HasExt(EGLExt extension) const {
    return mExtensions[extension];
}

}  // namespace dawn::native::opengl
