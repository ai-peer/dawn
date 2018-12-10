#include "dawn_native/RingBuffer.h"

namespace dawn_native {

    RingBufferBase::RingBufferBase(size_t maxSize) : mMaxSize(maxSize) {
    }

    // Free up memory tracked in the ring buffer by serial.
    void RingBufferBase::Tick(Serial lastCompletedSerial) {
        // Reclaim memory from previously recorded blocks.
        for (auto& request : mInflightRequests.IterateUpTo(lastCompletedSerial)) {
            mUsedStartOffset = request.endOffset;
            mUsedSize -= request.size;
        }

        // Dequeue previously recorded requests.
        mInflightRequests.ClearUpTo(lastCompletedSerial);
    }

    size_t RingBufferBase::GetMaxSize() const {
        return mMaxSize;
    }

    bool RingBufferBase::isEmpty() const {
        return mInflightRequests.Empty();
    }

    size_t RingBufferBase::SubAllocateBase(size_t allocSize) {
        size_t startOffset = -1;  // starting offset of the sub-alloc (default is invalid)

        if (mUsedSize >= mMaxSize)
            return startOffset;

        // Check if the buffer is NOT split (i.e sub-alloc on ends)
        if (mUsedStartOffset <= mUsedEndOffset) {
            // Order is important (try to sub-alloc at end first).
            if (mUsedEndOffset + allocSize <= mMaxSize) {
                startOffset = mUsedEndOffset;
                mUsedEndOffset += allocSize;
                mUsedSize += allocSize;
                mCurrentRequestSize += allocSize;
            }
            // Else, try to sub-alloc at front.
            else if (allocSize <= mUsedStartOffset) {
                startOffset = 0;
                mUsedEndOffset = allocSize;
                mUsedSize += allocSize;
                mCurrentRequestSize += allocSize;
            }
        }
        // Otherwise, buffer is split where sub-alloc must be in-between.
        else if (mUsedEndOffset + allocSize <= mUsedStartOffset) {
            startOffset = mUsedEndOffset;
            mUsedEndOffset += allocSize;
            mUsedSize += allocSize;
            mCurrentRequestSize += allocSize;
        }

        return startOffset;
    }
}  // namespace dawn_native