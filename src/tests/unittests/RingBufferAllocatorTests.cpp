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

#include <gtest/gtest.h>

#include "dawn_native/null/DeviceNull.h"

using namespace dawn_native;

class RingBufferTests : public testing::Test {
  protected:
    void SetUp() override {
        // TODO(bryan.bernhart@intel.com): Create this device through the adapter.
        mDevice = std::make_unique<null::Device>(/*adapter*/ nullptr, /*deviceDescriptor*/ nullptr);
    }

    null::Device* GetDevice() const {
        return mDevice.get();
    }

    std::unique_ptr<RingBuffer> CreateRingBuffer(size_t size) {
        std::unique_ptr<StagingBufferBase> stagingBuffer =
            std::make_unique<null::StagingBuffer>(size, mDevice.get());

        std::unique_ptr<RingBuffer> ringBuffer =
            std::make_unique<RingBuffer>(mDevice.get(), stagingBuffer.release());
        DAWN_UNUSED(ringBuffer->Initialize());
        return ringBuffer;
    }

  private:
    std::unique_ptr<null::Device> mDevice;
};

// Number of basic tests for Ringbuffer
TEST_F(RingBufferTests, BasicTest) {
    constexpr size_t sizeInBytes = 64000;
    std::unique_ptr<RingBuffer> buffer = CreateRingBuffer(sizeInBytes);

    // Ensure no requests exist on empty buffer.
    EXPECT_TRUE(buffer->Empty());

    ASSERT_EQ(buffer->GetSize(), sizeInBytes);

    // Ensure failure upon sub-allocating an oversized request.
    ASSERT_EQ(buffer->SubAllocate(sizeInBytes + 1), INVALID_OFFSET);

    // Fill the entire buffer with two requests of equal size.
    ASSERT_EQ(buffer->SubAllocate(sizeInBytes / 2), 0u);  // TODO: Fix offsets.
    ASSERT_EQ(buffer->SubAllocate(sizeInBytes / 2), 32000u);

    // Ensure the buffer is full.
    ASSERT_EQ(buffer->SubAllocate(1), INVALID_OFFSET);
}

// Tests that several ringbuffer allocations do not fail.
TEST_F(RingBufferTests, RingBufferManyAlloc) {
    constexpr size_t maxNumOfFrames = 64000;
    constexpr size_t frameSizeInBytes = 4;

    std::unique_ptr<RingBuffer> buffer = CreateRingBuffer(maxNumOfFrames * frameSizeInBytes);

    size_t offset = 0;
    for (size_t i = 0; i < maxNumOfFrames; ++i) {
        offset = buffer->SubAllocate(frameSizeInBytes);
        GetDevice()->Tick();
        ASSERT_EQ(offset, i * frameSizeInBytes);
    }
}

// Tests ringbuffer sub-allocations of the same serial are correctly tracked.
TEST_F(RingBufferTests, AllocInSameFrame) {
    constexpr size_t maxNumOfFrames = 3;
    constexpr size_t frameSizeInBytes = 4;

    std::unique_ptr<RingBuffer> buffer = CreateRingBuffer(maxNumOfFrames * frameSizeInBytes);

    //    F1
    //  [xxxx|--------]

    size_t offset = buffer->SubAllocate(frameSizeInBytes);
    GetDevice()->Tick();

    //    F1   F2
    //  [xxxx|xxxx|----]

    offset = buffer->SubAllocate(frameSizeInBytes);

    //    F1     F2
    //  [xxxx|xxxxxxxx]

    offset = buffer->SubAllocate(frameSizeInBytes);

    ASSERT_EQ(offset, 8u);
    ASSERT_EQ(buffer->GetUsedSize(), frameSizeInBytes * 3);

    buffer->Tick(1);

    // Used size does not change as previous sub-allocations were not tracked.
    ASSERT_EQ(buffer->GetUsedSize(), frameSizeInBytes * 3);

    buffer->Tick(2);

    ASSERT_EQ(buffer->GetUsedSize(), 0u);
    EXPECT_TRUE(buffer->Empty());
}

// Tests ringbuffer sub-allocation at various offsets.
TEST_F(RingBufferTests, RingBufferSubAlloc) {
    constexpr size_t maxNumOfFrames = 10;
    constexpr size_t frameSizeInBytes = 4;

    std::unique_ptr<RingBuffer> buffer = CreateRingBuffer(maxNumOfFrames * frameSizeInBytes);

    // Sub-alloc the first eight frames.
    for (size_t i = 0; i < 8; ++i) {
        buffer->SubAllocate(frameSizeInBytes);
        buffer->Track();
        GetDevice()->Tick();
    }

    // Each frame corrresponds to the serial number (for simplicity).
    //
    //    F1   F2   F3   F4   F5   F6   F7   F8
    //  [xxxx|xxxx|xxxx|xxxx|xxxx|xxxx|xxxx|xxxx|--------]
    //

    // Ensure an oversized allocation fails (only 8 bytes left)
    buffer->SubAllocate(frameSizeInBytes * 3);
    ASSERT_EQ(buffer->GetUsedSize(), frameSizeInBytes * 8);

    // Reclaim the first 3 frames.
    buffer->Tick(3);

    //                 F4   F5   F6   F7   F8
    //  [------------|xxxx|xxxx|xxxx|xxxx|xxxx|--------]
    //
    ASSERT_EQ(buffer->GetUsedSize(), frameSizeInBytes * 5);

    // Re-try the over-sized allocation.
    size_t offset = buffer->SubAllocate(frameSizeInBytes * 3);

    //        F9       F4   F5   F6   F7   F8
    //  [xxxxxxxxxxxx|xxxx|xxxx|xxxx|xxxx|xxxx|xxxxxxxx]
    //                                         ^^^^^^^^ wasted

    // In this example, Tick(8) could not reclaim the wasted bytes. The wasted bytes
    // were add to F9's sub-allocation.
    // TODO(bryan.bernhart@intel.com): Decide if Tick(8) should free these wasted bytes.

    ASSERT_EQ(offset, 0u);
    ASSERT_EQ(buffer->GetUsedSize(), frameSizeInBytes * maxNumOfFrames);

    // Ensure we are full.
    ASSERT_EQ(buffer->SubAllocate(frameSizeInBytes), INVALID_OFFSET);

    // Reclaim the next two frames.
    buffer->Tick(5);

    //        F9       F4   F5   F6   F7   F8
    //  [xxxxxxxxxxxx|----|----|xxxx|xxxx|xxxx|xxxxxxxx]
    //
    ASSERT_EQ(buffer->GetUsedSize(), frameSizeInBytes * 8);

    // Sub-alloc the chunk in the middle.
    offset = buffer->SubAllocate(frameSizeInBytes * 2);

    ASSERT_EQ(offset, frameSizeInBytes * 3);
    ASSERT_EQ(buffer->GetUsedSize(), frameSizeInBytes * maxNumOfFrames);

    //        F9                 F6   F7   F8
    //  [xxxxxxxxxxxx|xxxx|xxxx|xxxx|xxxx|xxxx|xxxxxxxx]
    //                ^^^^^^^^^ untracked

    // Ensure we are full.
    ASSERT_EQ(buffer->SubAllocate(frameSizeInBytes), INVALID_OFFSET);

    // Reclaim all.
    buffer->Tick(maxNumOfFrames);

    EXPECT_TRUE(buffer->Empty());
}
