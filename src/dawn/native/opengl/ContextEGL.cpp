// Copyright 2022 The Dawn Authors
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

#include "dawn/native/opengl/ContextEGL.h"

#include <memory>
#include <vector>

namespace dawn::native::opengl {

std::unique_ptr<ContextEGL> ContextEGL::Create(EGLFunctions& egl) {
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

    return std::make_unique<ContextEGL>(egl, display, context);
}

void ContextEGL::MakeCurrent() {
    egl.MakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, mContext);
}

}  // namespace dawn::native::opengl
