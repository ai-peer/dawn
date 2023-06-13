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

#ifndef SRC_DAWN_NATIVE_EVENTPIPE_H_
#define SRC_DAWN_NATIVE_EVENTPIPE_H_

#include <utility>
#include <vector>

#include "dawn/common/Platform.h"
#include "dawn/native/Error.h"
#include "dawn/native/IntegerTypes.h"

#if DAWN_PLATFORM_IS(WINDOWS)
typedef void* HANDLE;  // from windef.h
#endif

namespace dawn::native {

struct EventPrimitive {
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

struct EventPollInfo {
    EventPrimitive::T primitive;
    bool ready = false;
    size_t index = 0;
    bool alreadyCompleted = false;
};

class EventReceiver final {
  public:
    static ResultOrError<wgpu::WaitStatus> Poll(std::vector<EventPollInfo>* infos,
                                                Nanoseconds timeout);

    EventReceiver() = default;
    explicit EventReceiver(EventPrimitive::T);
    explicit EventReceiver(EventReceiver&&);
    EventReceiver& operator=(EventReceiver&&);
    ~EventReceiver();

    EventPrimitive::T Get() const;

  private:
    EventPrimitive mPrimitive;
};

class EventPipeSender final {
  public:
    static ResultOrError<std::pair<EventPipeSender, EventReceiver>> CreateEventPipe();

    EventPipeSender() = default;
    explicit EventPipeSender(EventPipeSender&&);
    EventPipeSender& operator=(EventPipeSender&&);
    ~EventPipeSender();

    MaybeError Signal();

  private:
    EventPrimitive mPrimitive;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_EVENTPIPE_H_
