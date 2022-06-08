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

#include <dawn/common/DynamicLib.h>
#include <dawn/native/opengl/EGLFunctions.h>

namespace dawn::native::opengl {

EGLFunctions::EGLFunctions() {
#if defined(DAWN_PLATFORM_WINDOWS)
    const char* eglLib = "libEGL.dll";
#elif defined(DAWN_PLATFORM_MACOS)
    const char* eglLib = "libEGL.dylib";
#else
    const char* eglLib = "libEGL.so";
#endif
    mLibEGL.Open(eglLib);
    ChooseConfig = reinterpret_cast<PFNEGLCHOOSECONFIGPROC>(LoadProc("eglChooseConfig"));
    CreateContext = reinterpret_cast<PFNEGLCREATECONTEXTPROC>(LoadProc("eglCreateContext"));
    CreatePbufferSurface =
        reinterpret_cast<PFNEGLCREATEPBUFFERSURFACEPROC>(LoadProc("eglCreatePbufferSurface"));
    CreatePlatformWindowSurface = reinterpret_cast<PFNEGLCREATEPLATFORMWINDOWSURFACEPROC>(
        LoadProc("eglCreatePlatformWindowSurface"));
    GetConfigs = reinterpret_cast<PFNEGLGETCONFIGSPROC>(LoadProc("eglGetConfigs"));
    GetCurrentDisplay =
        reinterpret_cast<PFNEGLGETCURRENTDISPLAYPROC>(LoadProc("eglGetCurrentDisplay"));
    GetDisplay = reinterpret_cast<PFNEGLGETDISPLAYPROC>(LoadProc("eglGetDisplay"));
    GetProcAddress = reinterpret_cast<PFNEGLGETPROCADDRESSPROC>(LoadProc("eglGetProcAddress"));
    Initialize = reinterpret_cast<PFNEGLINITIALIZEPROC>(LoadProc("eglInitialize"));
    MakeCurrent = reinterpret_cast<PFNEGLMAKECURRENTPROC>(LoadProc("eglMakeCurrent"));
}

void* EGLFunctions::LoadProc(const char* name) {
    return mLibEGL.GetProc(name);
}

}  // namespace dawn::native::opengl
