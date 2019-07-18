// Copyright 2019 The Dawn Authors
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

#include "dawn_native/EncodingContext.h"

#include "common/Assert.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"
#include "dawn_native/ErrorData.h"

namespace dawn_native {

    EncodingContext::EncodingContext(DeviceBase* device, const void* initialEncoder)
        : mDevice(device), mTopLevelEncoder(initialEncoder), mCurrentEncoder(initialEncoder) {
    }

    EncodingContext::~EncodingContext() {
        if (!mWereCommandsAcquired) {
            MoveToIterator();
            FreeCommands(&mIterator);
        }
    }

    // static
    EncodingContext* EncodingContext::MakeError(DeviceBase* device) {
        return new EncodingContext(device, nullptr);
    }

    CommandIterator EncodingContext::AcquireCommands() {
        ASSERT(!mWereCommandsAcquired);
        mWereCommandsAcquired = true;
        return std::move(mIterator);
    }

    void EncodingContext::MoveToIterator() {
        if (!mWasMovedToIterator) {
            mIterator = std::move(mAllocator);
            mWasMovedToIterator = true;
        }
    }

    CommandIterator* EncodingContext::GetIterator() {
        return &mIterator;
    }

    void EncodingContext::HandleError(const char* message) {
        if (!IsFinished()) {
            if (!mGotError) {
                mGotError = true;
                mErrorMessage = message;
            }
        } else {
            mDevice->HandleError(message);
        }
    }

    void EncodingContext::PopEncoder(const void* encoder) {
        // Assert we're not at the top level.
        ASSERT(mCurrentEncoder != mTopLevelEncoder);
        // Assert the popped encoder is current.
        ASSERT(mCurrentEncoder == encoder);

        mCurrentEncoder = mTopLevelEncoder;
    }

    void EncodingContext::PushEncoder(const void* encoder) {
        // Assert we're not at the top level.
        ASSERT(mCurrentEncoder == mTopLevelEncoder);
        ASSERT(encoder != nullptr);

        mCurrentEncoder = encoder;
    }

    MaybeError EncodingContext::Finish() {
        MaybeError err = [&]() -> MaybeError {
            if (mGotError) {
                return DAWN_VALIDATION_ERROR(mErrorMessage);
            }
            if (mCurrentEncoder != mTopLevelEncoder) {
                return DAWN_VALIDATION_ERROR("Command buffer recording ended mid-pass");
            }
            return {};
        }();

        // Even if finish validation fails, it is now invalid to call any encoding commands,
        // so we set its state to finished.
        mCurrentEncoder = nullptr;
        mTopLevelEncoder = nullptr;

        return err;
    }

    bool EncodingContext::IsFinished() const {
        return mTopLevelEncoder == nullptr;
    }

}  // namespace dawn_native
