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

#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/RingBufferD3D12.h"
#include "dawn_native/d3d12/d3d12_platform.h"

using namespace dawn_native::d3d12;

namespace {

    size_t ValidateValidUploadHandle(const dawn_native::UploadHandle& uploadHandle) {
        ASSERT(uploadHandle.mappedBuffer != nullptr);
        return uploadHandle.startOffset;
    }

    void ValidateInvalidUploadHandle(const dawn_native::UploadHandle& uploadHandle) {
        ASSERT_EQ(uploadHandle.mappedBuffer, nullptr);
    }
}  // namespace

class MemoryTests : public testing::Test {
  protected:
    void SetUp() override {
        mDevice = std::make_unique<Device>();
    }

    Device* GetDevice() const {
        return mDevice.get();
    }

    RingBuffer CreateRingBuffer(size_t size) {
        return RingBuffer(size, GetDevice());
    }

  private:
    std::unique_ptr<Device> mDevice;
};

// Number of basic tests for Ringbuffer
TEST_F(MemoryTests, BasicTest) {
    const size_t sizeInBytes = 64000;
    RingBuffer buffer = CreateRingBuffer(sizeInBytes);

    // Ensure no requests exist on empty buffer.
    ASSERT_TRUE(buffer.Empty());

    ASSERT_EQ(buffer.GetMaxSize(), sizeInBytes);

    // Ensure failure upon sub-allocating an oversized request.
    ValidateInvalidUploadHandle(buffer.SubAllocate(sizeInBytes + 1));

    // Fill the entire buffer with two requests of equal size.
    ValidateValidUploadHandle(buffer.SubAllocate(sizeInBytes / 2));
    ValidateValidUploadHandle(buffer.SubAllocate(sizeInBytes / 2));
    ASSERT(!buffer.Empty());

    // Ensure the buffer is full.
    ValidateInvalidUploadHandle(buffer.SubAllocate(1));
}

// Tests ringbuffer over-sized allocation fails.
TEST_F(MemoryTests, RingBufferLargeAlloc) {
    const size_t sizeInBytes = 64000;
    RingBuffer buffer = CreateRingBuffer(sizeInBytes);

    // Ensure failure upon sub-allocating an oversized request.
    ValidateInvalidUploadHandle(buffer.SubAllocate(sizeInBytes + 1));
}

// Tests that several ringbuffer allocations do not fail.
TEST_F(MemoryTests, RingBufferManyAlloc) {
    constexpr size_t maxNumOfFrames = 64000;
    constexpr size_t frameSizeInBytes = 4;

    RingBuffer buffer = CreateRingBuffer(maxNumOfFrames * frameSizeInBytes);

    size_t offset = 0;
    for (size_t i = 0; i < maxNumOfFrames; ++i) {
        offset = ValidateValidUploadHandle(buffer.SubAllocate(frameSizeInBytes));
        GetDevice()->Tick();
        ASSERT_EQ(offset, i * frameSizeInBytes);
    }
}

// Tests ringbuffer allocations at the front, middle and end.
TEST_F(MemoryTests, RingBufferAllocTest) {
    constexpr size_t maxNumOfFrames = 10;
    constexpr size_t frameSizeInBytes = 4;

    RingBuffer buffer = CreateRingBuffer(maxNumOfFrames * frameSizeInBytes);

    // Sub-alloc the first eight frames.
    for (size_t i = 0; i < 8; ++i) {
        ValidateValidUploadHandle(buffer.SubAllocate(frameSizeInBytes));
        GetDevice()->Tick();
    }

    // Each frame corrresponds to the serial number (for simplicity).
    // Note: the first frame (or serial) was submitted by the device upon creation.
    //
    //    F2   F3   F4   F5   F6   F7   F8   F9
    //  [xxxx|xxxx|xxxx|xxxx|xxxx|xxxx|xxxx|xxxx|--------]
    //

    // Ensure an oversized allocation fails (only 8 bytes left)
    ValidateInvalidUploadHandle(buffer.SubAllocate(frameSizeInBytes * 3));

    // Reclaim the first 3 frames.
    buffer.Tick(4);

    //                 F5   F6   F7   F8   F9
    //  [------------|xxxx|xxxx|xxxx|xxxx|xxxx|--------]
    //

    // Re-try the over-sized allocation.
    size_t offset = ValidateValidUploadHandle(buffer.SubAllocate(frameSizeInBytes * 3));

    //       F10       F5   F6   F7   F8   F9
    //  [xxxxxxxxxxxx|xxxx|xxxx|xxxx|xxxx|xxxx|xxxxxxxx]
    //                                         ^^^^^^^^ wasted
    ASSERT_EQ(offset, 0u);

    // Ensure we are full.
    ValidateInvalidUploadHandle(buffer.SubAllocate(frameSizeInBytes));

    // Reclaim the next two frames.
    buffer.Tick(6);

    //       F10       F5   F6   F7   F8   F9
    //  [xxxxxxxxxxxx|----|----|xxxx|xxxx|xxxx|xxxxxxxx]
    //

    // Sub-alloc the chunk in the middle.
    offset = ValidateValidUploadHandle(buffer.SubAllocate(frameSizeInBytes * 2));

    ASSERT_EQ(offset, frameSizeInBytes * 3);

    // Ensure we are full.
    ValidateInvalidUploadHandle(buffer.SubAllocate(frameSizeInBytes));

    // Reclaim all.
    buffer.Tick(maxNumOfFrames);

    ASSERT(buffer.Empty());
}