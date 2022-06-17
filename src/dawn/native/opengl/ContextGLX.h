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

#ifndef SRC_DAWN_NATIVE_OPENGL_CONTEXTGLX_H_
#define SRC_DAWN_NATIVE_OPENGL_CONTEXTGLX_H_

#include <memory>

#include "src/dawn/common/xlib_with_undefs.h"

#include "dawn/native/opengl/DeviceGL.h"
#include "dawn/native/opengl/GLXFunctions.h"

namespace dawn::native::opengl {

class ContextGLX : public Device::Context {
  public:
    static std::unique_ptr<ContextGLX> Create(GLXFunctions& functions);
    void MakeCurrent() override;
    ContextGLX(GLXFunctions& functions, Display* display, GLXDrawable drawable, GLXContext context);
    ~ContextGLX() override;

  private:
    GLXFunctions glX;
    Display* mDisplay;
    GLXDrawable mDrawable;
    GLXContext mContext;
};

}  // namespace dawn::native::opengl

#endif  // SRC_DAWN_NATIVE_OPENGL_CONTEXTEGL_H_
