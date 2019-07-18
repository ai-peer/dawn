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

#ifndef DAWNNATIVE_COMMANDRECORDER_H_
#define DAWNNATIVE_COMMANDRECORDER_H_

#include "dawn_native/dawn_platform.h"

#include "dawn_native/CommandAllocator.h"
#include "dawn_native/Error.h"
#include "dawn_native/ObjectBase.h"

#include <string>

namespace dawn_native {

    // Base class for allocating/iterating commands and tracking errors.
    class CommandRecorder : public ObjectBase {
      public:
        CommandRecorder(DeviceBase* device);
        ~CommandRecorder();

        CommandIterator AcquireCommands();

        // Functions to interact with the encoders
        void HandleError(const char* message);
        void ConsumeError(ErrorData* error);
        bool ConsumedError(MaybeError maybeError) {
            if (DAWN_UNLIKELY(maybeError.IsError())) {
                ConsumeError(maybeError.AcquireError());
                return true;
            }
            return false;
        }

      protected:
        void MoveToIterator();
        CommandAllocator mAllocator;
        CommandIterator mIterator;
        bool mWasMovedToIterator = false;
        bool mWereCommandsAcquired = false;

        bool mIsRecording = true;
        bool mGotError = false;
        std::string mErrorMessage;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_COMMANDRECORDER_H_
