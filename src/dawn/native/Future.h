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

#ifndef SRC_DAWN_NATIVE_FUTURE_H_
#define SRC_DAWN_NATIVE_FUTURE_H_

#include <mutex>
#include <string>
#include <utility>

#include "dawn/native/Error.h"
#include "dawn/native/Forward.h"
#include "dawn/native/IntegerTypes.h"
#include "dawn/native/ObjectBase.h"
#include "dawn/webgpu_cpp.h"

namespace dawn::native {

wgpu::WaitStatus APIFuturesWaitAny(size_t* futureCount, FutureBase** futures, uint64_t timeout);
void APIFuturesGetEarliestFds(size_t count, FutureBase* const* futures, int* fds);

class FutureBase : public ApiObjectBase {
  public:
    explicit FutureBase(DeviceBase*);
    ~FutureBase() override = default;

    virtual std::optional<std::pair<QueueBase*, ExecutionSerial>> GetQueueSerial() {
        return std::nullopt;
    }
    virtual PosixFd GetFd() const = 0;
    virtual PosixFd GetFdInternal() const { UNREACHABLE(); }
    virtual MaybeError Signal() = 0;
    virtual ResultOrError<wgpu::WaitStatus> Wait(Milliseconds timeout) = 0;

    bool IsReady() const;

  protected:
    enum class State {
        Pending,
        Ready,
        Observed,
    };
    State mState = State::Pending;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_FUTURE_H_
