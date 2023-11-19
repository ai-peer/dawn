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

#include "dawn/native/d3d/KeyedMutexHelper.h"

#include <utility>

#include "dawn/native/D3DBackend.h"
#include "dawn/native/d3d/D3DError.h"

namespace dawn::native::d3d {
KeyedMutexGuard::KeyedMutexGuard() = default;
KeyedMutexGuard::KeyedMutexGuard(KeyedMutexGuard&&) = default;
KeyedMutexGuard& KeyedMutexGuard::operator=(KeyedMutexGuard&&) = default;

KeyedMutexGuard::KeyedMutexGuard(KeyedMutexHelper* keyedMutexHelper)
    : mKeyedMutexHelper(keyedMutexHelper) {}

KeyedMutexGuard::~KeyedMutexGuard() {
    if (mKeyedMutexHelper != nullptr) {
        mKeyedMutexHelper->ReleaseKeyedMutex();
    }
}

KeyedMutexHelper::KeyedMutexHelper(ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex)
    : mDXGIKeyedMutex(std::move(dxgiKeyedMutex)) {
    DAWN_CHECK(mDXGIKeyedMutex);
}

KeyedMutexHelper::~KeyedMutexHelper() {
    DAWN_CHECK(mAccessCount == 0);
}

ResultOrError<KeyedMutexGuard> KeyedMutexHelper::AcquireKeyedMutex() {
    if (mAccessCount == 0) {
        DAWN_TRY(CheckHRESULT(mDXGIKeyedMutex->AcquireSync(kDXGIKeyedMutexAcquireKey, INFINITE),
                              "Failed to acquire keyed mutex for external image"));
    }
    mAccessCount++;
    return KeyedMutexGuard(this);
}

void KeyedMutexHelper::ReleaseKeyedMutex() {
    mAccessCount--;
    if (mAccessCount == 0) {
        mDXGIKeyedMutex->ReleaseSync(kDXGIKeyedMutexAcquireKey);
    }
}
}  // namespace dawn::native::d3d
