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

#include "dawn_native/DynamicUploader.h"

namespace dawn_native {

    UploadHandle DynamicUploader::Allocate(uint32_t size, uint32_t alignment) {
        // Check that the alignment is a power-of-two.
        const size_t alignmentMask = alignment - 1;

        ASSERT((alignmentMask & alignment) == 0);

        // Align the requested allocation size
        const size_t alignedSize = (size + alignmentMask) & ~alignmentMask;

        RingBufferBase* largestRingBuffer = GetLargestBuffer();
        UploadHandle uploadHandle = largestRingBuffer->SubAllocate(alignedSize);

        // Upon failure, append a newly created (and much larger) ring buffer to fulfill the
        // request.
        if (uploadHandle.mappedBuffer == nullptr) {
            // Compute the new max size (in powers of two to preserve alignment).
            size_t newMaxSize = largestRingBuffer->GetSize() << 1;
            for (; newMaxSize < size; newMaxSize *= 2)
                ;

            CreateBuffer(newMaxSize);
            uploadHandle = GetLargestBuffer()->SubAllocate(alignedSize);
        }

        return uploadHandle;
    }

    void DynamicUploader::Tick(Serial lastCompletedSerial) {
        // Reclaim memory within the ring buffers by ticking (or removing requests no longer
        // in-flight).
        for (size_t i = 0; i < mRingBuffers.size(); ++i) {
            mRingBuffers[i]->Tick(lastCompletedSerial);

            // Never erase the last (or largest buffer) as to prevent re-creating smaller buffers
            // again.
            if (mRingBuffers[i]->Empty() && i < mRingBuffers.size() - 1) {
                mRingBuffers.erase(mRingBuffers.begin() + i);
            }
        }
    }
}  // namespace dawn_native