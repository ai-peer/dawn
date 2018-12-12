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

    bool RingBufferBase::Empty() const {
        return mInflightRequests.Empty();
    }

    UploadHandle RingBufferBase::SubAllocate(size_t allocSize) {
        UploadHandle uploadHandle;
        uploadHandle.mappedBuffer = nullptr;

        if (mUsedSize >= mMaxSize)
            return uploadHandle;

        size_t startOffset = -1;  // starting offset of the sub-alloc (default is invalid)

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
                // Count the space at front in the request size so that a subsequent
                // sub-alloc cannot not succeed when the buffer is full.
                const size_t requestSize = (mMaxSize - mUsedEndOffset) + allocSize;

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

        if (startOffset == static_cast<size_t>(-1))
            return uploadHandle;

        // Track new request when serial advances.
        const Serial currentSerial = GetPendingCommandSerial();
        if (mInflightRequests.Empty() || currentSerial > mInflightRequests.LastSerial()) {
            Request request;
            request.endOffset = mUsedEndOffset;
            request.size = mCurrentRequestSize;

            mInflightRequests.Enqueue(std::move(request), currentSerial);
            mCurrentRequestSize = 0;  // reset
        }

        uploadHandle.mappedBuffer = GetCPUVirtualAddressPointer() + startOffset;
        uploadHandle.startOffset = startOffset;

        return uploadHandle;
    }
}  // namespace dawn_native