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

#include "src/dawn/native/SystemHandle.h"

#include <utility>

#include "dawn/common/Log.h"

namespace dawn::native {

SystemHandle::SystemHandle() : mHandle(kInvalidHandle<T>) {}

SystemHandle::SystemHandle(T handle) : mHandle(handle) {}

bool SystemHandle::IsValid() const {
    return IsHandleValid(mHandle);
}

SystemHandle::SystemHandle(SystemHandle&& rhs) {
    mHandle = rhs.mHandle;
    rhs.mHandle = kInvalidHandle<T>;
}

SystemHandle& SystemHandle::operator=(SystemHandle&& rhs) {
    if (this != &rhs) {
        std::swap(mHandle, rhs.mHandle);
    }
    return *this;
}

SystemHandle::T SystemHandle::Get() const {
    return mHandle;
}

SystemHandle::T SystemHandle::Detach() {
    T handle = mHandle;
    mHandle = kInvalidHandle<T>;
    return handle;
}

ResultOrError<SystemHandle> SystemHandle::Duplicate() const {
    T handle;
    DAWN_TRY_ASSIGN(handle, DuplicateHandle(mHandle));
    return SystemHandle(handle);
}

void SystemHandle::Close() {
    DAWN_ASSERT(IsValid());
    auto result = CloseHandle(mHandle);
    // Still invalidate the handle if Close failed.
    // If Close failed, the handle surely was invalid already.
    mHandle = kInvalidHandle<T>;
    if (DAWN_UNLIKELY(result.IsError())) {
        dawn::ErrorLog() << result.AcquireError()->GetFormattedMessage();
    }
}

SystemHandle::~SystemHandle() {
    if (IsValid()) {
        Close();
    }
}

}  // namespace dawn::native
