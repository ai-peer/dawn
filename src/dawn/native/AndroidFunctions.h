// Copyright 2023 The Dawn & Tint Authors
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

#ifndef SRC_DAWN_NATIVE_ANDROIDFUNCTIONS_H_
#define SRC_DAWN_NATIVE_ANDROIDFUNCTIONS_H_

#include "dawn/common/DynamicLib.h"

#include <android/hardware_buffer.h>

extern "C" {
using PFN_AHardwareBuffer_acquire = void (*)(AHardwareBuffer* buffer);
using PFN_AHardwareBuffer_release = void (*)(AHardwareBuffer* buffer);
using PFN_AHardwareBuffer_describe = void (*)(const AHardwareBuffer* buffer,
                                              AHardwareBuffer_Desc* outDesc);
}

class DynamicLib;

namespace dawn::native {

// A helper class that dynamically loads libandroid.so that might not be present
// on all platforms Dawn is deployed on.
class AndroidFunctions {
  public:
    AndroidFunctions();
    ~AndroidFunctions();

    bool IsLoaded() const;

    PFN_AHardwareBuffer_acquire AHardwareBuffer_acquire = nullptr;
    PFN_AHardwareBuffer_release AHardwareBuffer_release = nullptr;
    PFN_AHardwareBuffer_describe AHardwareBuffer_describe = nullptr;

  private:
    DynamicLib mAndroidLib;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_ANDROIDFUNCTIONS_H_
