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

#include "dawn/native/Error.h"
#include "dawn/native/IntegerTypes.h"

struct pollfd;

namespace dawn::native {

ResultOrError<int> Poll(std::vector<pollfd>* pollfds, Nanoseconds timeout);

class EventReceiver final {
  public:
    EventReceiver() = default;
    explicit EventReceiver(PosixFd);
    explicit EventReceiver(EventReceiver&&);
    EventReceiver& operator=(EventReceiver&&);
    ~EventReceiver();

    PosixFd GetFd() const;

  private:
    PosixFd mFd{-1};
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
    int mFd = -1;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_EVENTPIPE_H_
