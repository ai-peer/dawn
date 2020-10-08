// Copyright 2018 The Dawn Authors
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

#include "dawn_native/Fence.h"

#include "common/Assert.h"
#include "dawn_native/Device.h"
#include "dawn_native/ValidationUtils_autogen.h"

#include <utility>

namespace dawn_native {

    struct FenceInFlight : QueueBase::TaskInFlight {
        FenceInFlight(Ref<Fence> fence, FenceAPISerial value)
            : fence(std::move(fence)), value(value) {
        }
        void Finish() override {
            fence->SetCompletedValue(value);
        }
        ~FenceInFlight() override = default;

      private:
        Ref<Fence> fence;
        FenceAPISerial value;
    };

    MaybeError ValidateFenceDescriptor(const FenceDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }

        return {};
    }

    // Fence

    Fence::Fence(QueueBase* queue, const FenceDescriptor* descriptor)
        : ObjectBase(queue->GetDevice()),
          mSignalValue(descriptor->initialValue),
          mCompletedValue(descriptor->initialValue),
          mQueue(queue) {
        mFenceSignalTracker = std::make_unique<FenceSignalTracker>(queue->GetDevice());
    }

    Fence::Fence(DeviceBase* device, ObjectBase::ErrorTag tag) : ObjectBase(device, tag) {
        mFenceSignalTracker = std::make_unique<FenceSignalTracker>(device);
    }

    Fence::~Fence() {
        for (auto& request : mRequests.IterateAll()) {
            ASSERT(!IsError());
            request.completionCallback(WGPUFenceCompletionStatus_Unknown, request.userdata);
        }
        mRequests.Clear();
    }

    Fence::FenceSignalTracker::FenceSignalTracker(DeviceBase* device) : mDevice(device) {
    }

    void Fence::FenceSignalTracker::UpdateFenceOnComplete(Fence* fence, FenceAPISerial value) {
        std::unique_ptr<FenceInFlight> fenceInFlight =
            std::make_unique<FenceInFlight>(fence, value);
        // If there is pending future callback work, we use the pending callback serial so that
        // we wait for the next serial to be completed before we update the fence completed
        // value. Without pending future callback work, we can use the last submitted serial
        // because we only have a single queue, we can update the fence
        // completed value once the last submitted serial has passed
        bool hasFutureCallbackWork =
            mDevice->GetFutureCallbackSerial() >= mDevice->GetPendingCommandSerial();
        ExecutionSerial serial = hasFutureCallbackWork ? mDevice->GetPendingCommandSerial()
                                                       : mDevice->GetLastSubmittedCommandSerial();
        mDevice->GetDefaultQueue()->TrackTask(std::move(fenceInFlight), serial);
    }

    // static
    Fence* Fence::MakeError(DeviceBase* device) {
        return new Fence(device, ObjectBase::kError);
    }

    uint64_t Fence::GetCompletedValue() const {
        if (IsError()) {
            return 0;
        }
        return uint64_t(mCompletedValue);
    }

    void Fence::OnCompletion(uint64_t apiValue,
                             wgpu::FenceOnCompletionCallback callback,
                             void* userdata) {
        FenceAPISerial value(apiValue);

        WGPUFenceCompletionStatus status;
        if (GetDevice()->ConsumedError(ValidateOnCompletion(value, &status))) {
            callback(status, userdata);
            return;
        }
        ASSERT(!IsError());

        if (value <= mCompletedValue) {
            callback(WGPUFenceCompletionStatus_Success, userdata);
            return;
        }

        OnCompletionData request;
        request.completionCallback = callback;
        request.userdata = userdata;
        mRequests.Enqueue(std::move(request), value);
    }

    FenceAPISerial Fence::GetSignaledValue() const {
        ASSERT(!IsError());
        return mSignalValue;
    }

    const QueueBase* Fence::GetQueue() const {
        ASSERT(!IsError());
        return mQueue.Get();
    }

    void Fence::SetSignaledValue(FenceAPISerial signalValue) {
        ASSERT(!IsError());
        ASSERT(signalValue > mSignalValue);
        mSignalValue = signalValue;
    }

    void Fence::SetCompletedValue(FenceAPISerial completedValue) {
        ASSERT(!IsError());
        ASSERT(completedValue <= mSignalValue);
        ASSERT(completedValue > mCompletedValue);
        mCompletedValue = completedValue;

        for (auto& request : mRequests.IterateUpTo(mCompletedValue)) {
            if (GetDevice()->IsLost()) {
                request.completionCallback(WGPUFenceCompletionStatus_DeviceLost, request.userdata);
            } else {
                request.completionCallback(WGPUFenceCompletionStatus_Success, request.userdata);
            }
        }
        mRequests.ClearUpTo(mCompletedValue);
    }

    MaybeError Fence::ValidateOnCompletion(FenceAPISerial value,
                                           WGPUFenceCompletionStatus* status) const {
        *status = WGPUFenceCompletionStatus_DeviceLost;
        DAWN_TRY(GetDevice()->ValidateIsAlive());

        *status = WGPUFenceCompletionStatus_Error;
        DAWN_TRY(GetDevice()->ValidateObject(this));

        if (value > mSignalValue) {
            return DAWN_VALIDATION_ERROR("Value greater than fence signaled value");
        }

        *status = WGPUFenceCompletionStatus_Success;
        return {};
    }

}  // namespace dawn_native
