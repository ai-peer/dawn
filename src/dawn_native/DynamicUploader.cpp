// Copyright 2017 The Dawn Authors
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

#include "dawn_native/DynamicUploader.h"

namespace dawn_native {

    DynamicUploader::~DynamicUploader() {
        // Only a single ring-buffer (the largest) may persist after Tick().
        ASSERT(mRingBuffers.empty() <= 1);
    }

    UploadHandle DynamicUploader::Allocate(uint32_t size, uint32_t alignment) {
        // Align the requested allocation size
        const size_t alignmentMask = alignment - 1;
        size_t alignedSize = (alignmentMask) ? (size + alignmentMask) & ~alignmentMask : size;

        std::unique_ptr<RingBufferBase>& largestRingBuffer = GetBuffer();
        UploadHandle uploadHandle = largestRingBuffer->SubAllocate(alignedSize);

        // Upon failure, append a newly created (and much larger) ring buffer to fulfill the
        // request.
        if (uploadHandle.mappedBuffer == nullptr) {
            // Compute the new max size (in powers of two to preserve alignment requirements).
            size_t newMaxSize = largestRingBuffer->GetMaxSize() << 1;
            for (; newMaxSize < size; newMaxSize *= 2)
                ;

            CreateBuffer(newMaxSize);
            uploadHandle = GetBuffer()->SubAllocate(alignedSize);
        }

        return uploadHandle;
    }

    void DynamicUploader::Tick(Serial lastCompletedSerial) {
        // Reclaim ring buffers by ticking (or removing completed in-flight requests)
        // then check if the buffer is empty. Delete only the [0, ith] buffers in sequence
        // as the ith buffer corresponds to the ith frame.
        size_t count = 0;
        for (size_t i = 0; i < mRingBuffers.size(); ++i) {
            mRingBuffers[i]->Tick(lastCompletedSerial);

            // Never erase the last (or largest buffer), as it will keep track of the max size.
            if (mRingBuffers[i]->isEmpty() && i == count && i < mRingBuffers.size() - 1)
                count++;
        }

        if (count > 0) {
            ASSERT(count < mRingBuffers.size());
            mRingBuffers.erase(mRingBuffers.begin(), mRingBuffers.begin() + count);
        }
    }
}  // namespace dawn_native