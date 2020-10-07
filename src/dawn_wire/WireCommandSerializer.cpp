// Copyright 2020 The Dawn Authors
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

#include "dawn_wire/Wire.h"

#include "common/Assert.h"

namespace dawn_wire {

    CommandSerializer::CommandSerializer(size_t maxCommandSize) : mMaxCommandSize(maxCommandSize) {
    }

    char* CommandSerializer::GetCmdSpaceInternal(size_t size, bool* isOwned) {
        ASSERT(isOwned != nullptr);
        if (size > mMaxCommandSize || mIsDisconnected) {
            *isOwned = !mIsDisconnected;
            mOwnedCmdSpace.resize(size);
            return mOwnedCmdSpace.data();
        }
        *isOwned = false;
        return static_cast<char*>(GetCmdSpace(size));
    }

    void CommandSerializer::SerializeOwnedCmdSpace() {
        const char* src = mOwnedCmdSpace.data();
        size_t remainingSize = mOwnedCmdSpace.size();
        while (remainingSize > 0 && !mIsDisconnected) {
            size_t size = std::min(remainingSize, mMaxCommandSize);
            void* dst = GetCmdSpace(size);
            memcpy(dst, src, size);

            src += size;
            remainingSize -= size;
        }
        // Clear the buffer because we hit this function when the command is huge.
        // We don't want to hold onto this memory for a long time.
        mOwnedCmdSpace.clear();
    }

    bool CommandSerializer::IsDisconnected() const {
        return mIsDisconnected;
    }

    void CommandSerializer::Disconnect() {
        if (mIsDisconnected) {
            return;
        }
        mIsDisconnected = true;
    }

}  // namespace dawn_wire
