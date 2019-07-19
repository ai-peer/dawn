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

#include "dawn_native/CommandRecorder.h"

#include "common/Assert.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"
#include "dawn_native/ErrorData.h"

namespace dawn_native {

    CommandRecorder::CommandRecorder(DeviceBase* device) : ObjectBase(device) {
    }

    CommandRecorder::~CommandRecorder() {
        if (!mWereCommandsAcquired) {
            MoveToIterator();
            FreeCommands(&mIterator);
        }
    }

    CommandIterator CommandRecorder::AcquireCommands() {
        ASSERT(!mWereCommandsAcquired);
        mWereCommandsAcquired = true;
        return std::move(mIterator);
    }

    void CommandRecorder::MoveToIterator() {
        if (!mWasMovedToIterator) {
            mIterator = std::move(mCommandAllocator);
            mWasMovedToIterator = true;
        }
    }

    void CommandRecorder::HandleError(const char* message) {
        if (mIsRecording) {
            if (!mGotError) {
                mGotError = true;
                mErrorMessage = message;
            }
        } else {
            GetDevice()->HandleError(message);
        }
    }

    void CommandRecorder::ConsumeError(ErrorData* error) {
        HandleError(error->GetMessage().c_str());
        delete error;
    }

}  // namespace dawn_native
