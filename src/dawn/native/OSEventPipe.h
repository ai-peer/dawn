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

#ifndef SRC_DAWN_NATIVE_OSEVENTPIPE_H_
#define SRC_DAWN_NATIVE_OSEVENTPIPE_H_

#include <utility>

#include "dawn/native/OSEventReceiver.h"

namespace dawn::native {

// OSEventPipe provides an OSEventReceiver that can be signalled by Dawn code.
// This is useful for queue completions on Metal (where Metal signals us by calling a callback)
// and for async pipeline creations that happen in a worker-thread task.
//
// We use OS events even for these because, unlike C++/pthreads primitives (mutexes, atomics,
// condvars, etc.), it's possible to wait-any on them (wait for any of a list of events to fire).
// Other use-cases in Dawn that don't require wait-any should generally use C++ primitives, for
// example for signalling the completion of other types of worker-thread work that don't need to
// signal a WGPUFuture.
//
// OSEventReceiver is one-time-use (see OSEventReceiver), so there's no way to reset an OSEventPipe.
//
// - On Windows, OSEventReceiver is a Win32 Event Object, so we can create one with CreateEvent()
//   and signal it with SetEvent().
// - On POSIX, OSEventReceiver is a file descriptor (fd), so we can create one with pipe(), and
//   signal it by write()ing into the pipe (to make it become readable, though we won't read() it).
class OSEventPipe final {
  public:
    static std::pair<OSEventPipe, OSEventReceiver> CreateEventPipe();

    OSEventPipe() = default;
    explicit OSEventPipe(OSEventPipe&&);
    OSEventPipe& operator=(OSEventPipe&&);
    ~OSEventPipe();

    void Signal();

  private:
    OSEventPrimitive mPrimitive;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_OSEVENTPIPE_H_
