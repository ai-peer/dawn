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

#ifndef SRC_DAWN_NATIVE_OSEVENT_H_
#define SRC_DAWN_NATIVE_OSEVENT_H_

#include <span>
#include <utility>

#include "dawn/common/Platform.h"
#include "dawn/native/Error.h"
#include "dawn/native/IntegerTypes.h"

#if DAWN_PLATFORM_IS(WINDOWS)
typedef void* HANDLE;  // from windef.h
#endif

namespace dawn::native {

struct TrackedFuturePollInfo;

struct OSEventPrimitive {
#if DAWN_PLATFORM_IS(WINDOWS)
    using T = HANDLE;
    T v = nullptr;
#else
    using T = int;
    T v = -1;
#endif
    bool IsValid() const;
    void Close();
};

class OSEventReceiver final {
  public:
    // Returns true if some future is now ready, false if not (it timed out).
    static ResultOrError<bool> Wait(size_t count,
                                    TrackedFuturePollInfo* futures,
                                    Nanoseconds timeout);

    OSEventReceiver() = default;
    explicit OSEventReceiver(OSEventPrimitive::T);
    explicit OSEventReceiver(OSEventReceiver&&);
    OSEventReceiver& operator=(OSEventReceiver&&);
    ~OSEventReceiver();

    OSEventPrimitive::T Get() const;

  private:
    OSEventPrimitive mPrimitive;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_OSEVENT_H_
