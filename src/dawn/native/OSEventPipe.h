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

#include "dawn/native/Error.h"
#include "dawn/native/OSEvent.h"

namespace dawn::native {

class OSEventPipe final {
  public:
    static ResultOrError<std::pair<OSEventPipe, OSEventReceiver>> CreateEventPipe();

    OSEventPipe() = default;
    explicit OSEventPipe(OSEventPipe&&);
    OSEventPipe& operator=(OSEventPipe&&);
    ~OSEventPipe();

    MaybeError Signal();

  private:
    OSEventPrimitive mPrimitive;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_OSEVENTPIPE_H_
