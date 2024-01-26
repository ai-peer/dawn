// Copyright 2024 The Dawn & Tint Authors
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

#include "dawn/native/d3d/KeyedMutex.h"

#include <utility>

#include "dawn/native/D3DBackend.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d/DeviceD3D.h"

namespace dawn::native::d3d {

KeyedMutex::Guard::Guard(Guard&& other) {
    *this = std::move(other);
}

KeyedMutex::Guard& KeyedMutex::Guard::operator=(Guard&& other) {
    Reset();
    mKeyedMutex = std::move(other.mKeyedMutex);
    return *this;
}

KeyedMutex::Guard::Guard(Ref<KeyedMutex> keyedMutex) : mKeyedMutex(std::move(keyedMutex)) {
    DAWN_CHECK(mKeyedMutex != nullptr);
}

KeyedMutex::Guard::~Guard() {
    Reset();
}

void KeyedMutex::Guard::Reset() {
    if (mKeyedMutex != nullptr) {
        mKeyedMutex->ReleaseKeyedMutex();
    }
}

KeyedMutex::KeyedMutex(ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex, Device* device)
    : mDXGIKeyedMutex(std::move(dxgiKeyedMutex)), mDevice(device) {
    DAWN_CHECK(mDXGIKeyedMutex);
    DAWN_CHECK(mDevice);
}

KeyedMutex::~KeyedMutex() {
    DAWN_CHECK(mAcquireCount == 0);
    mDevice->DisposeKeyedMutex(std::move(mDXGIKeyedMutex));
}

ResultOrError<KeyedMutex::Guard> KeyedMutex::AcquireKeyedMutex() {
    DAWN_CHECK(mAcquireCount >= 0);
    if (mAcquireCount == 0) {
        DAWN_TRY(CheckHRESULT(mDXGIKeyedMutex->AcquireSync(kDXGIKeyedMutexAcquireKey, INFINITE),
                              "Failed to acquire keyed mutex for external image"));
    }
    mAcquireCount++;
    return KeyedMutex::Guard(this);
}

void KeyedMutex::ReleaseKeyedMutex() {
    DAWN_CHECK(mAcquireCount > 0);
    mAcquireCount--;
    if (mAcquireCount == 0) {
        mDXGIKeyedMutex->ReleaseSync(kDXGIKeyedMutexAcquireKey);
    }
}

}  // namespace dawn::native::d3d
