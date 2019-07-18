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
#include "dawn_native/ErrorData.h"

#include <string>

namespace dawn_native {

    // Base class for allocating/iterating commands.
    // It performs error tracking as well as encoding state for render/compute passes/
    class EncodingContext {
      public:
        EncodingContext(DeviceBase* device, const void* initialEncoder);
        ~EncodingContext();

        static EncodingContext* MakeError(DeviceBase* device);

        CommandIterator AcquireCommands();
        CommandIterator* GetIterator();
        void MoveToIterator();

        // Functions to handle encoder errors
        void HandleError(const char* message);

        inline void ConsumeError(ErrorData* error) {
            HandleError(error->GetMessage().c_str());
            delete error;
        }

        inline bool ConsumedError(MaybeError maybeError) {
            if (DAWN_UNLIKELY(maybeError.IsError())) {
                ConsumeError(maybeError.AcquireError());
                return true;
            }
            return false;
        }

        template <typename EncodeFunction>
        inline bool TryEncode(const void* encoder, EncodeFunction&& encodeFunction) {
            if (DAWN_UNLIKELY(encoder != mCurrentEncoder)) {
                if (mCurrentEncoder != mTopLevelEncoder) {
                    // The top level encoder was used when a pass encoder was current.
                    HandleError("Command cannot be recorded inside a pass");
                } else {
                    HandleError("Recording in an error or already ended pass encoder");
                }
                return true;
            }
            return ConsumedError(encodeFunction(&mAllocator));
        }

        // Functions to set current encoder state
        void PopEncoder(const void* encoder);
        void PushEncoder(const void* encoder);
        MaybeError Finish();

        bool IsFinished() const;

      private:
        DeviceBase* mDevice;

        // There can only be two levels of encoders. Top-level and render/compute pass.
        // The top level encoder is the encoder the EncodingContext is created with.
        // It doubles as flag to check if encoding has been Finished.
        const void* mTopLevelEncoder;
        // The current encoder must be the same as the encoder provided to TryEncode,
        // otherwise an error is produced. It may be nullptr if the EncodingContext is an error.
        // The current encoder changes with Push/PopEncoder which should be called by Begin/EndPass.
        const void* mCurrentEncoder;

        CommandAllocator mAllocator;
        CommandIterator mIterator;
        bool mWasMovedToIterator = false;
        bool mWereCommandsAcquired = false;

        bool mGotError = false;
        std::string mErrorMessage;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_COMMANDRECORDER_H_
