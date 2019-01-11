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

#ifndef DAWNWIRE_WIREDESERIALIZEALLOCATOR_H_
#define DAWNWIRE_WIREDESERIALIZEALLOCATOR_H_

#include "dawn_wire/WireCmd_autogen.h"

#include <algorithm>
#include <vector>

namespace dawn_wire {
    // A really really simple implementation of the DeserializeAllocator. It's main feature
    // is that it has some inline storage so as to avoid allocations for the majority of
    // commands.
    class WireDeserializeAllocator : public DeserializeAllocator {
      public:
        WireDeserializeAllocator() {
            Reset();
        }

        ~WireDeserializeAllocator() {
            Reset();
        }

        void* GetSpace(size_t size) override {
            // Return space in the current buffer if possible first.
            if (mRemainingSize >= size) {
                char* buffer = mCurrentBuffer;
                mCurrentBuffer += size;
                mRemainingSize -= size;
                return buffer;
            }

            // Otherwise allocate a new buffer and try again.
            size_t allocationSize = std::max(size, size_t(2048));
            char* allocation = static_cast<char*>(malloc(allocationSize));
            if (allocation == nullptr) {
                return nullptr;
            }

            mAllocations.push_back(allocation);
            mCurrentBuffer = allocation;
            mRemainingSize = allocationSize;
            return GetSpace(size);
        }

        void Reset() {
            for (auto allocation : mAllocations) {
                free(allocation);
            }
            mAllocations.clear();

            // The initial buffer is the inline buffer so that some allocations can be skipped
            mCurrentBuffer = mStaticBuffer;
            mRemainingSize = sizeof(mStaticBuffer);
        }

      private:
        size_t mRemainingSize = 0;
        char* mCurrentBuffer = nullptr;
        char mStaticBuffer[2048];
        std::vector<char*> mAllocations;
    };
}  // namespace dawn_wire

#endif  // DAWNWIRE_WIREDESERIALIZEALLOCATOR_H_
