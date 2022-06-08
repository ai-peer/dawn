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

#include <EGL/egl.h>

#include <dawn/common/DynamicLib.h>

namespace dawn::native::opengl {

struct EGLFunctions {
    explicit EGLFunctions(PFNEGLGETPROCADDRESSPROC getProcAddress);
    PFNEGLCHOOSECONFIGPROC ChooseConfig;
    PFNEGLCREATECONTEXTPROC CreateContext;
    PFNEGLCREATEPLATFORMWINDOWSURFACEPROC CreatePlatformWindowSurface;
    PFNEGLCREATEPBUFFERSURFACEPROC CreatePbufferSurface;
    PFNEGLGETCONFIGSPROC GetConfigs;
    PFNEGLGETCURRENTDISPLAYPROC GetCurrentDisplay;
    PFNEGLGETDISPLAYPROC GetDisplay;
    PFNEGLGETPROCADDRESSPROC GetProcAddress;
    PFNEGLINITIALIZEPROC Initialize;
    PFNEGLMAKECURRENTPROC MakeCurrent;
};

}  // namespace dawn::native::opengl
