// Copyright 2019 The Dawn Authors
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

#include "dawn/native/opengl/BackendGL.h"

#include <EGL/egl.h>

#include <utility>

#include "dawn/common/SystemUtils.h"
#include "dawn/native/Instance.h"
#include "dawn/native/OpenGLBackend.h"
#include "dawn/native/opengl/AdapterGL.h"
#include "dawn/native/opengl/EGLFunctions.h"

namespace dawn::native::opengl {

namespace {

EGLContext CreateEGLContext(EGLFunctions& egl) {
    EGLDisplay display = egl.GetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint num_config;
    egl.Initialize(display, nullptr, nullptr);
    EGLint config_attribs[] = {EGL_RED_SIZE,
                               8,
                               EGL_GREEN_SIZE,
                               8,
                               EGL_BLUE_SIZE,
                               8,
                               EGL_ALPHA_SIZE,
                               8,
                               EGL_DEPTH_SIZE,
                               24,
                               EGL_STENCIL_SIZE,
                               8,
                               EGL_SAMPLES,
                               4,
                               EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES3_BIT,
                               EGL_SURFACE_TYPE,
                               EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                               EGL_NONE};

    if (egl.ChooseConfig(display, config_attribs, nullptr, 0, &num_config) == EGL_FALSE) {
        return nullptr;
    }

    if (num_config == 0) {
        return nullptr;
    }

    std::vector<EGLConfig> configs(num_config);
    if (egl.ChooseConfig(display, config_attribs, configs.data(), num_config, &num_config) ==
        EGL_FALSE) {
        return nullptr;
    }

    EGLConfig config = configs[0];  // FIXME
    EGLint attrib_list[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE, EGL_NONE,
    };
    EGLContext context = egl.CreateContext(display, config, EGL_NO_CONTEXT, attrib_list);
    if (!context) {
        return nullptr;
    }

    if (egl.MakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context) == EGL_FALSE) {
        return nullptr;
    }

    return context;
}

}  // anonymous namespace

// Implementation of the OpenGL backend's BackendConnection

Backend::Backend(InstanceBase* instance, wgpu::BackendType backendType)
    : BackendConnection(instance, backendType) {}

Backend::~Backend() {
    if (mContext) {
        egl.DestroyContext(egl.GetDisplay(EGL_DEFAULT_DISPLAY), mContext);
    }
}

std::vector<Ref<AdapterBase>> Backend::DiscoverDefaultAdapters() {
    std::vector<Ref<AdapterBase>> adapters;
#ifdef DAWN_ENABLE_BACKEND_OPENGLES
    if (GetType() == wgpu::BackendType::OpenGLES) {
#if DAWN_PLATFORM_IS(WINDOWS)
        const char* eglLib = "libEGL.dll";
#elif DAWN_PLATFORM_IS(MACOS)
        const char* eglLib = "libEGL.dylib";
#else
        const char* eglLib = "libEGL.so";
#endif
        DynamicLib libEGL;
        if (!libEGL.Open(eglLib)) {
            return {};
        }

        AdapterDiscoveryOptionsES options;
        options.getProc =
            reinterpret_cast<void* (*)(const char*)>(libEGL.GetProc("eglGetProcAddress"));
        if (!options.getProc) {
            return {};
        }

        egl.Init(reinterpret_cast<PFNEGLGETPROCADDRESSPROC>(options.getProc));
        if (!mContext) {
            mContext = CreateEGLContext(egl);
        }

        if (!mContext) {
            return {};
        }

        auto result = DiscoverAdapters(&options);
        if (result.IsError()) {
            GetInstance()->ConsumedError(result.AcquireError());
        } else {
            auto value = result.AcquireSuccess();
            adapters.insert(adapters.end(), value.begin(), value.end());
        }
    }
#endif
    return adapters;
}

ResultOrError<std::vector<Ref<AdapterBase>>> Backend::DiscoverAdapters(
    const AdapterDiscoveryOptionsBase* optionsBase) {
    // TODO(cwallez@chromium.org): For now only create a single OpenGL adapter because don't
    // know how to handle MakeCurrent.
    DAWN_INVALID_IF(mCreatedAdapter, "The OpenGL backend can only create a single adapter.");

    ASSERT(static_cast<wgpu::BackendType>(optionsBase->backendType) == GetType());
    const AdapterDiscoveryOptions* options =
        static_cast<const AdapterDiscoveryOptions*>(optionsBase);

    DAWN_INVALID_IF(options->getProc == nullptr, "AdapterDiscoveryOptions::getProc must be set");

    Ref<Adapter> adapter = AcquireRef(
        new Adapter(GetInstance(), static_cast<wgpu::BackendType>(optionsBase->backendType)));
    DAWN_TRY(adapter->InitializeGLFunctions(options->getProc));
    DAWN_TRY(adapter->Initialize());

    mCreatedAdapter = true;
    std::vector<Ref<AdapterBase>> adapters{std::move(adapter)};
    return std::move(adapters);
}

BackendConnection* Connect(InstanceBase* instance, wgpu::BackendType backendType) {
    return new Backend(instance, backendType);
}

}  // namespace dawn::native::opengl
