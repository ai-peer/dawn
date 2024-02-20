// Copyright 2019 The Dawn & Tint Authors
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

#ifndef SRC_DAWN_NATIVE_OPENGL_OPENGLFUNCTIONS_H_
#define SRC_DAWN_NATIVE_OPENGL_OPENGLFUNCTIONS_H_

#include <string>

#include "dawn/native/opengl/OpenGLFunctionsBase_autogen.h"
#include "dawn/native/opengl/OpenGLVersion.h"

namespace dawn::native::opengl {

class Device;

struct OpenGLFunctions : OpenGLFunctionsBase {
  public:
    MaybeError Initialize(GetProcAddress getProc);

    const OpenGLVersion& GetVersion() const;
    bool IsAtLeastGL(uint32_t majorVersion, uint32_t minorVersion) const;
    bool IsAtLeastGLES(uint32_t majorVersion, uint32_t minorVersion) const;

  private:
    OpenGLVersion mVersion;
};

struct OpenGLFunctionsScopedWrapper {
  public:
    OpenGLFunctionsScopedWrapper(const OpenGLFunctions& functions, const Device* device);
    ~OpenGLFunctionsScopedWrapper();
    OpenGLFunctionsScopedWrapper(OpenGLFunctionsScopedWrapper&&) = default;
    OpenGLFunctionsScopedWrapper& operator=(OpenGLFunctionsScopedWrapper&&) = default;

    const OpenGLFunctions& GetGLFunctions() const { return mFunctions; }

  private:
    OpenGLFunctions mFunctions;
    // TODO(blundell): Call through to `mDevice` to restore previous context
    // when this wrapper object is destroyed (note: also need to null out
    // `mDevice` when std::move'ing this object to another object to avoid
    // spurious calls). Note that this object can't take in Device::Context
    // because DeviceGL.h needs to include this file. Could move
    // OpenGLFunctionsScopedWrapper to a different file to work around that.
    [[maybe_unused]] const Device* mDevice;
};

}  // namespace dawn::native::opengl

#endif  // SRC_DAWN_NATIVE_OPENGL_OPENGLFUNCTIONS_H_
