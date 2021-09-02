// Copyright 2021 The Dawn Authors
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

#ifndef DAWNNATIVE_VALIDATIONSCRATCHBUFFER_H_
#define DAWNNATIVE_VALIDATIONSCRATCHBUFFER_H_

#include "common/RefCounted.h"
#include "dawn_native/Buffer.h"

#include <cstdint>

namespace dawn_native {

    class DeviceBase;

    // Per-Device lazily allocated and lazily grown scratch buffer for on-GPU validation work.
    // Any programmable pass that needs to schedule on-GPU validation can use this helper to
    // allocate space in the buffer and bind it for use in the validation pass(es) as well as in
    // any corresponding indirect draw or dispatch calls.
    class ValidationScratchBuffer {
      public:
        explicit ValidationScratchBuffer(DeviceBase* device);
        ~ValidationScratchBuffer();

        // Resets the buffer to an empty state with at least `capacity` bytes of capacity. If
        // necessary the underlying Buffer is replaced with a new one.
        MaybeError Reset(uint64_t capacity);

        // Allocates the next available `numBytes` of the buffer and returns its offset. The total
        // number of bytes allocated since the most recent Reset() must not exceed its specified
        // capacity.
        uint64_t Claim(uint64_t numBytes);

        BufferBase* GetBuffer() const {
            ASSERT(mBuffer.Get() != nullptr);
            return mBuffer.Get();
        }

      private:
        DeviceBase* const mDevice;

        Ref<BufferBase> mBuffer;
        uint64_t mNumOccupiedBytes = 0;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_VALIDATIONSCRATCHBUFFER_H_
