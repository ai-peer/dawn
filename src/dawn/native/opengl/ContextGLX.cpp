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

#include "dawn/native/opengl/ContextGLX.h"

#include <memory>
#include <vector>
#include "dawn/common/DynamicLib.h"

namespace dawn::native::opengl {

typedef Display* (*PFN_XOpenDisplay)(const char*);

std::unique_ptr<ContextGLX> ContextGLX::Create(GLXFunctions& functions) {
    DynamicLib libX11;
    if (!libX11.Open("libX11.so.6")) {
        return nullptr;
    }
    PFN_XOpenDisplay XOpenDisplay =
        reinterpret_cast<PFN_XOpenDisplay>(libX11.GetProc("XOpenDisplay"));
    if (!XOpenDisplay) {
        return nullptr;
    }
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        return nullptr;
    }
    XSetWindowAttributes wa;
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    Window window = XCreateWindow(display, root, 0, 0, 1, 1, 0, 0, InputOnly,
                                  DefaultVisual(display, screen), CWEventMask, &wa);
    if (!window) {
        return nullptr;
    }
    return nullptr;
}

ContextGLX::ContextGLX(GLXFunctions& functions,
                       Display* display,
                       GLXDrawable drawable,
                       GLXContext context)
    : glX(functions), mDisplay(display), mDrawable(drawable), mContext(context) {}

void ContextGLX::MakeCurrent() {
    glX.MakeCurrent(mDisplay, mDrawable, mContext);
}

ContextGLX::~ContextGLX() {
    glX.DestroyContext(mDisplay, mContext);
}

}  // namespace dawn::native::opengl
