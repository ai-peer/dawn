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

#include "dawn/native/ExecutionQueue.h"

namespace dawn::native {

ExecutionSerial ExecutionQueueBase::GetPendingCommandSerial() const {
    return ExecutionSerial(mLastSubmittedSerial.load(std::memory_order_acquire) + 1);
}

ExecutionSerial ExecutionQueueBase::GetLastSubmittedCommandSerial() const {
    return ExecutionSerial(mLastSubmittedSerial.load(std::memory_order_acquire));
}

ExecutionSerial ExecutionQueueBase::GetCompletedCommandSerial() const {
    return ExecutionSerial(mCompletedSerial.load(std::memory_order_acquire));
}

MaybeError ExecutionQueueBase::CheckPassedSerials() {
    ExecutionSerial completedSerial;
    DAWN_TRY_ASSIGN(completedSerial, CheckAndUpdateCompletedSerials());

    uint64_t completionSerialValue = static_cast<uint64_t>(completedSerial);

    DAWN_ASSERT(completionSerialValue <= mLastSubmittedSerial);
    // completedSerial should not be less than mCompletedSerial unless it is 0.
    // It can be 0 when there's no fences to check.
    DAWN_ASSERT(completionSerialValue >= mCompletedSerial || completionSerialValue == 0);

    uint64_t current = mCompletedSerial.load();
    while (completionSerialValue > current &&
           !mCompletedSerial.compare_exchange_weak(current, completionSerialValue)) {
    }
    return {};
}

void ExecutionQueueBase::AssumeCommandsComplete() {
    // Bump serials so any pending callbacks can be fired.
    uint64_t prev = mLastSubmittedSerial.fetch_add(1u, std::memory_order_release);
    mCompletedSerial.store(prev + 1, std::memory_order_release);
}

void ExecutionQueueBase::IncrementLastSubmittedCommandSerial() {
    mLastSubmittedSerial.fetch_add(1u, std::memory_order_release);
}

bool ExecutionQueueBase::HasScheduledCommands() const {
    return mLastSubmittedSerial.load(std::memory_order_acquire) >
               mCompletedSerial.load(std::memory_order_acquire) ||
           HasPendingCommands();
}

// All prevously submitted works at the moment will supposedly complete at this serial.
// Internally the serial is computed according to whether frontend and backend have pending
// commands. There are 4 cases of combination:
//   1) Frontend(No), Backend(No)
//   2) Frontend(No), Backend(Yes)
//   3) Frontend(Yes), Backend(No)
//   4) Frontend(Yes), Backend(Yes)
// For case 1, we don't need the serial to track the task as we can ack it right now.
// For case 2 and 4, there will be at least an eventual submission, so we can use
// 'GetPendingCommandSerial' as the serial.
// For case 3, we can't use 'GetPendingCommandSerial' as it won't be submitted surely. Instead we
// use 'GetLastSubmittedCommandSerial', which must be fired eventually.
ExecutionSerial ExecutionQueueBase::GetScheduledWorkDoneSerial() const {
    return HasPendingCommands() ? GetPendingCommandSerial() : GetLastSubmittedCommandSerial();
}

}  // namespace dawn::native
