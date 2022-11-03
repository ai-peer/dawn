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

#ifndef SRC_DAWN_NATIVE_OPENGL_EGLFUNCTIONS_H_
#define SRC_DAWN_NATIVE_OPENGL_EGLFUNCTIONS_H_

#include <EGL/egl.h>

// Potentially redefinition of typedefs that should be identical to those in egl.h when available.
// This set of redefinitions are necessary though because they may not be available depending on
// the EGL library.
typedef EGLBoolean(EGLAPIENTRYP PFNEGLBINDAPIPROC)(EGLenum api);
typedef EGLBoolean(EGLAPIENTRYP PFNEGLCHOOSECONFIGPROC)(EGLDisplay dpy,
                                                        const EGLint* attrib_list,
                                                        EGLConfig* configs,
                                                        EGLint config_size,
                                                        EGLint* num_config);
typedef EGLContext(EGLAPIENTRYP PFNEGLCREATECONTEXTPROC)(EGLDisplay dpy,
                                                         EGLConfig config,
                                                         EGLContext share_context,
                                                         const EGLint* attrib_list);
typedef EGLSurface(EGLAPIENTRYP PFNEGLCREATEPLATFORMWINDOWSURFACEPROC)(
    EGLDisplay dpy,
    EGLConfig config,
    void* native_window,
    const EGLAttrib* attrib_list);
typedef EGLSurface(EGLAPIENTRYP PFNEGLCREATEPBUFFERSURFACEPROC)(EGLDisplay dpy,
                                                                EGLConfig config,
                                                                const EGLint* attrib_list);
typedef EGLBoolean(EGLAPIENTRYP PFNEGLDESTROYCONTEXTPROC)(EGLDisplay dpy, EGLContext ctx);
typedef EGLBoolean(EGLAPIENTRYP PFNEGLGETCONFIGSPROC)(EGLDisplay dpy,
                                                      EGLConfig* configs,
                                                      EGLint config_size,
                                                      EGLint* num_config);
typedef EGLContext(EGLAPIENTRYP PFNEGLGETCURRENTCONTEXTPROC)(void);
typedef EGLDisplay(EGLAPIENTRYP PFNEGLGETCURRENTDISPLAYPROC)(void);
typedef EGLSurface(EGLAPIENTRYP PFNEGLGETCURRENTSURFACEPROC)(EGLint readdraw);
typedef EGLDisplay(EGLAPIENTRYP PFNEGLGETDISPLAYPROC)(EGLNativeDisplayType display_id);
typedef EGLint(EGLAPIENTRYP PFNEGLGETERRORPROC)(void);
typedef __eglMustCastToProperFunctionPointerType(EGLAPIENTRYP PFNEGLGETPROCADDRESSPROC)(
    const char* procname);
typedef EGLBoolean(EGLAPIENTRYP PFNEGLINITIALIZEPROC)(EGLDisplay dpy, EGLint* major, EGLint* minor);
typedef EGLBoolean(EGLAPIENTRYP PFNEGLMAKECURRENTPROC)(EGLDisplay dpy,
                                                       EGLSurface draw,
                                                       EGLSurface read,
                                                       EGLContext ctx);
typedef const char*(EGLAPIENTRYP PFNEGLQUERYSTRINGPROC)(EGLDisplay dpy, EGLint name);

namespace dawn::native::opengl {

struct EGLFunctions {
    void Init(void* (*getProc)(const char*));
    PFNEGLBINDAPIPROC BindAPI;
    PFNEGLCHOOSECONFIGPROC ChooseConfig;
    PFNEGLCREATECONTEXTPROC CreateContext;
    PFNEGLCREATEPLATFORMWINDOWSURFACEPROC CreatePlatformWindowSurface;
    PFNEGLCREATEPBUFFERSURFACEPROC CreatePbufferSurface;
    PFNEGLDESTROYCONTEXTPROC DestroyContext;
    PFNEGLGETCONFIGSPROC GetConfigs;
    PFNEGLGETCURRENTCONTEXTPROC GetCurrentContext;
    PFNEGLGETCURRENTDISPLAYPROC GetCurrentDisplay;
    PFNEGLGETCURRENTSURFACEPROC GetCurrentSurface;
    PFNEGLGETDISPLAYPROC GetDisplay;
    PFNEGLGETERRORPROC GetError;
    PFNEGLGETPROCADDRESSPROC GetProcAddress;
    PFNEGLINITIALIZEPROC Initialize;
    PFNEGLMAKECURRENTPROC MakeCurrent;
    PFNEGLQUERYSTRINGPROC QueryString;
};

}  // namespace dawn::native::opengl

#endif  // SRC_DAWN_NATIVE_OPENGL_EGLFUNCTIONS_H_
