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

#include "dawn/native/metal/QueueWorkDoneFutureMTL.h"

#include "dawn/native/ToBackend.h"
#include "dawn/native/metal/DeviceMTL.h"

namespace dawn::native::metal {

QueueWorkDoneFuture::QueueWorkDoneFuture(QueueBase* queue, ExecutionSerial serial)
    : QueueWorkDoneFutureBase(queue, serial) {}

ResultOrError<wgpu::WaitStatus> QueueWorkDoneFuture::Wait(Milliseconds timeout) {
    Device* device = ToBackend(GetDevice());
    DAWN_TRY(device->WaitKeventForSerial(mSerial, timeout));
    ASSERT(uint64_t(timeout) == UINT64_MAX);
    return wgpu::WaitStatus::SomeCompleted;
}

}  // namespace dawn::native::metal
