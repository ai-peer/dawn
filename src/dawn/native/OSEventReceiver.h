// Copyright 2023 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_OSEVENTRECEIVER_H_
#define SRC_DAWN_NATIVE_OSEVENTRECEIVER_H_

#include <span>
#include <utility>

#include "dawn/common/NonCopyable.h"
#include "dawn/common/Platform.h"
#include "dawn/native/Error.h"
#include "dawn/native/IntegerTypes.h"

namespace dawn::native {

struct TrackedFutureWaitInfo;

struct OSEventPrimitive {
#if DAWN_PLATFORM_IS(WINDOWS)
    using T = void*;  // void* is the underlying type of Win32's HANDLE.
    T v = nullptr;
#elif DAWN_PLATFORM_IS(POSIX)
    using T = int;
    T v = -1;
#endif
    bool IsValid() const;
    void Close();
};

// OSEventReceiver holds an OS event primitive (Win32 Event Object or POSIX file descriptor (fd)
// that will be signalled by some other thing: either an OS integration like SetEventOnCompletion(),
// or our own code like OSEventPipe.
//
// OSEventReceiver is one-time-use (to make it easier to use correctly) - once it's been signalled,
// it won't ever get reset (become unsignalled). Instead, if we want to reuse underlying OS objects,
// it should be reset and recycled *after* the OSEventReceiver and OSEventPipe have been destroyed.
class OSEventReceiver final : NonCopyable {
  public:
    static OSEventReceiver CreateAlreadySignaled();
    // Returns true if some future is now ready, false if not (it timed out).
    [[nodiscard]] static bool WaitAny(size_t count,
                                      TrackedFutureWaitInfo* futures,
                                      Nanoseconds timeout);

    OSEventReceiver() = default;
    explicit OSEventReceiver(OSEventPrimitive::T);
    OSEventReceiver(OSEventReceiver&&);
    OSEventReceiver& operator=(OSEventReceiver&&);
    ~OSEventReceiver();

    OSEventPrimitive::T Get() const;

  private:
    OSEventPrimitive mPrimitive;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_OSEVENTRECEIVER_H_
