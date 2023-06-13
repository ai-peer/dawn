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

#include "dawn/native/Future.h"

namespace dawn::native {

wgpu::WaitStatus APIWaitAnyFutures(size_t futureCount,
                                   FutureBase* const* futures,
                                   uint64_t timeout) {
    for (size_t i = 0; i < futureCount; ++i) {
        wgpu::WaitStatus status = futures[i]->Wait(timeout);
        if (status == wgpu::WaitStatus::NonePending) {
            continue;
        }
        return status;
    }
    return wgpu::WaitStatus::NonePending;
}

FutureBase::FutureBase(DeviceBase* device) : ApiObjectBase(device, "") {}

}  // namespace dawn::native
