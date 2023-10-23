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

#ifndef SRC_DAWN_NATIVE_SYSTEMHANDLE_H_
#define SRC_DAWN_NATIVE_SYSTEMHANDLE_H_

#include "dawn/common/NonCopyable.h"
#include "dawn/common/Platform.h"
#include "dawn/native/Error.h"

#if DAWN_PLATFORM_IS(WINDOWS)
#include "dawn/common/windows_with_undefs.h"
#elif DAWN_PLATFORM_IS(FUCHSIA)
#include <zircon/syscalls.h>
#elif DAWN_PLATFORM_IS(POSIX)
#include <unistd.h>
#endif

namespace dawn::native {

template <typename T>
inline T kInvalidHandle;

// Default implementation of IsHandleValid which returns false.
template <typename T>
inline bool IsHandleValid(T handle) {
    return false;
}

// Default implementation of DuplicateHandle which generates an error.
template <typename T>
inline ResultOrError<T> DuplicateHandle(T handle) {
    return DAWN_UNIMPLEMENTED_ERROR("Not implemented.");
}

// Default implementation of CloseHandle which generates an error.
template <typename T>
inline MaybeError CloseHandle(T handle) {
    return DAWN_UNIMPLEMENTED_ERROR("Not implemented.");
}

class SystemHandle : public NonCopyable {
  public:
    // Create an alias for the actual primitive handle type on a platform.
#if DAWN_PLATFORM_IS(WINDOWS)
    using T = HANDLE;
#elif DAWN_PLATFORM_IS(FUCHSIA)
    using T = zx_handle_t;
#elif DAWN_PLATFORM_IS(POSIX)
    using T = int;
#endif

    SystemHandle();

    // Create a SystemHandle by taking ownership of `handle`.
    // Use template and SFINAE to prevent implicit conversion of primitive handles.
    template <typename Arg, typename = std::enable_if_t<std::is_same_v<Arg, T>>>
    static SystemHandle Acquire(Arg handle) {
        return SystemHandle(handle);
    }

    // Create a SystemHandle by duplicating `handle`.
    // Use template and SFINAE to prevent implicit conversion of primitive handles.
    template <typename Arg, typename = std::enable_if_t<std::is_same_v<Arg, T>>>
    static ResultOrError<SystemHandle> Duplicate(Arg handle) {
        T outHandle;
        DAWN_TRY_ASSIGN(outHandle, DuplicateHandle(handle));
        return SystemHandle(outHandle);
    }

    bool IsValid() const;

    SystemHandle(SystemHandle&& rhs);
    SystemHandle& operator=(SystemHandle&& rhs);

    T Get() const;
    T Detach();
    ResultOrError<SystemHandle> Duplicate() const;
    void Close();

    ~SystemHandle();

  private:
    explicit SystemHandle(T handle);

    T mHandle;
};

#if DAWN_PLATFORM_IS(WINDOWS)

template <>
constexpr inline HANDLE kInvalidHandle<HANDLE> = nullptr;

template <>
inline bool IsHandleValid(HANDLE handle) {
    return handle != nullptr;
}

template <>
inline ResultOrError<HANDLE> DuplicateHandle(HANDLE handle) {
    HANDLE currentProcess = ::GetCurrentProcess();
    HANDLE outHandle;
    if (DAWN_UNLIKELY(!::DuplicateHandle(currentProcess, handle, currentProcess, &outHandle, 0,
                                         FALSE, DUPLICATE_SAME_ACCESS))) {
        return DAWN_INTERNAL_ERROR("DuplicateHandle failed");
    }
    return outHandle;
}

template <>
inline MaybeError CloseHandle(HANDLE handle) {
    if (DAWN_LIKELY(::CloseHandle(handle))) {
        return {};
    }
    return DAWN_INTERNAL_ERROR("CloseHandle failed");
}

#elif DAWN_PLATFORM_IS(FUCHSIA)

template <>
constexpr inline zx_handle_t kInvalidHandle<zx_handle_t> = 0;

template <>
inline bool IsHandleValid(zx_handle_t handle) {
    return handle > 0;
}

template <>
inline ResultOrError<zx_handle_t> DuplicateHandle(zx_handle_t handle) {
    zx_handle_t outHandle = ZX_HANDLE_INVALID;
    zx_status_t status = zx_handle_duplicate(descriptor->handle, ZX_RIGHT_SAME_RIGHTS, &outHandle);
    if (DAWN_UNLIKELY(status != ZK_OK)) {
        return DAWN_INTERNAL_ERROR("zx_handle_duplicate failed");
    }
    return outHandle;
}

template <>
inline bool CloseHandle(zx_handle_t handle) {
    zx_status_t status = zx_handle_close(handle);
    if (DAWN_UNLIKELY(status != ZX_OK)) {
        return DAWN_INTERNAL_ERROR("zx_handle_close failed");
    }
    return {};
}

#elif DAWN_PLATFORM_IS(POSIX)

template <>
constexpr inline int kInvalidHandle<int> = -1;

template <>
inline bool IsHandleValid(int handle) {
    return handle >= 0;
}

template <>
inline ResultOrError<int> DuplicateHandle(int handle) {
    int outHandle = dup(handle);
    if (DAWN_UNLIKELY(outHandle < 0)) {
        return DAWN_INTERNAL_ERROR("dup failed");
    }
    return outHandle;
}

template <>
inline MaybeError CloseHandle(int handle) {
    if (DAWN_UNLIKELY(close(handle) < 0)) {
        return DAWN_INTERNAL_ERROR("close failed");
    }
    return {};
}

#endif

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_SYSTEMHANDLE_H_
