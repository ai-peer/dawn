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

#include "dawn_native/RingBuffer.h"

namespace dawn_native {

    static constexpr size_t INVALID_OFFSET = -1;

    RingBufferBase::RingBufferBase(size_t size) : mBufferSize(size) {
    }

    // Record allocations in a request when serial advances.
    // This method has been splitted from Tick() for testing.
    void RingBufferBase::Track() {
        if (mCurrentRequestSize == 0)
            return;
        const Serial currentSerial = GetPendingCommandSerial();
        if (mInflightRequests.Empty() || currentSerial > mInflightRequests.LastSerial()) {
            Request request;
            request.endOffset = mUsedEndOffset;
            request.size = mCurrentRequestSize;

            mInflightRequests.Enqueue(std::move(request), currentSerial);
            mCurrentRequestSize = 0;  // reset
        }
    }

    void RingBufferBase::Tick(Serial lastCompletedSerial) {
        Track();

        // Reclaim memory from previously recorded blocks.
        for (auto& request : mInflightRequests.IterateUpTo(lastCompletedSerial)) {
            mUsedStartOffset = request.endOffset;
            mUsedSize -= request.size;
        }

        // Dequeue previously recorded requests.
        mInflightRequests.ClearUpTo(lastCompletedSerial);
    }

    size_t RingBufferBase::GetSize() const {
        return mBufferSize;
    }

    size_t RingBufferBase::GetUsedSize() const {
        return mUsedSize;
    }

    bool RingBufferBase::Empty() const {
        return mInflightRequests.Empty();
    }

    // Sub-allocate the ring-buffer by requesting a chunk of the specified size.
    // This is a serial-based resource scheme, the life-span of resources (and the allocations) get
    // tracked by GPU progress via serials. Memory can be reused by determining if the GPU has
    // completed up to a given serial. Each sub-allocation request is tracked in the serial offset
    // queue, which identifies an existing (or new) frames-worth of resources. Internally, the
    // ring-buffer maintains offsets of 3 "memory" states: Free, Reclaimed, and Used. This is done
    // in FIFO order as older frames would free resources before newer ones.
    UploadHandle RingBufferBase::SubAllocate(size_t allocSize) {
        UploadHandle uploadHandle;
        uploadHandle.mappedBuffer = nullptr;

        // Check if the buffer is full by comparing the used size.
        // If the buffer is not split where waste occurs (e.g. cannot fit new sub-alloc in front), a
        // subsequent sub-alloc could fail where the used size was previously adjusted to include
        // the wasted.
        if (mUsedSize >= mBufferSize)
            return uploadHandle;

        size_t startOffset = INVALID_OFFSET;

        // Check if the buffer is NOT split (i.e sub-alloc on ends)
        if (mUsedStartOffset <= mUsedEndOffset) {
            // Order is important (try to sub-alloc at end first).
            // This is due to FIFO order where sub-allocs are inserted from left-to-right (when not
            // wrapped).
            if (mUsedEndOffset + allocSize <= mBufferSize) {
                startOffset = mUsedEndOffset;
                mUsedEndOffset += allocSize;
                mUsedSize += allocSize;
                mCurrentRequestSize += allocSize;
            }
            // Else, try to sub-alloc at front.
            else if (allocSize <= mUsedStartOffset) {
                // Count the space at front in the request size so that a subsequent
                // sub-alloc cannot not succeed when the buffer is full.
                const size_t requestSize = (mBufferSize - mUsedEndOffset) + allocSize;

                startOffset = 0;
                mUsedEndOffset = allocSize;
                mUsedSize += requestSize;
                mCurrentRequestSize += requestSize;
            }
        }
        // Otherwise, buffer is split where sub-alloc must be in-between.
        else if (mUsedEndOffset + allocSize <= mUsedStartOffset) {
            startOffset = mUsedEndOffset;
            mUsedEndOffset += allocSize;
            mUsedSize += allocSize;
            mCurrentRequestSize += allocSize;
        }

        if (startOffset == INVALID_OFFSET)
            return uploadHandle;

        uploadHandle.mappedBuffer = GetCPUVirtualAddressPointer() + startOffset;
        uploadHandle.startOffset = startOffset;

        return uploadHandle;
    }
}  // namespace dawn_native