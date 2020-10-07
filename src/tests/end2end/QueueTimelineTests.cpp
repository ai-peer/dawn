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

#include <gmock/gmock.h>
#include "tests/DawnTest.h"

class QueueTimelineTests : public DawnTest {
  protected:
    struct Done {
        bool onCompleteDone = false;
        bool mapDone = false;
    };

    void WaitForCompletedValue(wgpu::Fence fence, uint64_t completedValue) {
        while (fence.GetCompletedValue() < completedValue) {
            WaitABit();
        }
    }

    wgpu::Buffer CreateAndWriteToBuffer(uint32_t myData, size_t kSize) {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = 4;
        descriptor.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
        wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

        queue.WriteBuffer(buffer, 0, &myData, kSize);
        return buffer;
    }

  private:
};

// Test that buffer.MapAsync callback happens before fence.OnCompletion callback
// when queue.Signal is called after buffer.MapAsync. The callback order should
// happen in the order the functions are called.
TEST_P(QueueTimelineTests, MapReadSignalOnComplete) {
    uint32_t myData = 0x01020304;
    constexpr size_t kSize = sizeof(myData);
    wgpu::Buffer buffer = CreateAndWriteToBuffer(myData, kSize);
    Done done;

    buffer.MapAsync(
        wgpu::MapMode::Read, 0, kSize,
        [](WGPUBufferMapAsyncStatus status, void* userdata) {
            EXPECT_EQ(WGPUBufferMapAsyncStatus_Success, status);
            // fence.OnCompletion callback should not have ran yet, onCompleteDone should still be
            // false.
            EXPECT_EQ(static_cast<Done*>(userdata)->onCompleteDone, false);
            static_cast<Done*>(userdata)->mapDone = true;
        },
        &done);

    wgpu::Fence fence = queue.CreateFence();

    queue.Signal(fence, 1);
    fence.OnCompletion(
        1u,
        [](WGPUFenceCompletionStatus status, void* userdata) {
            EXPECT_EQ(WGPUFenceCompletionStatus_Success, status);
            // buffer.MapAsync callback should have set mapDone to true
            EXPECT_EQ(static_cast<Done*>(userdata)->mapDone, true);
            static_cast<Done*>(userdata)->onCompleteDone = true;
        },
        &done);
    WaitForCompletedValue(fence, 1);

    EXPECT_EQ(myData, *static_cast<const uint32_t*>(buffer.GetConstMappedRange()));
    buffer.Unmap();
}

// Test that fence.OnCompletion callback happens before buffer.MapAsync callback when
// queue.Signal is called before buffer.MapAsync. The callback order should
// happen in the order the functions are called.
TEST_P(QueueTimelineTests, SignalMapReadOnComplete) {
    uint32_t myData = 0x01020304;
    constexpr size_t kSize = sizeof(myData);
    wgpu::Buffer buffer = CreateAndWriteToBuffer(myData, kSize);

    Done done;

    wgpu::Fence fence = queue.CreateFence();
    queue.Signal(fence, 2);

    buffer.MapAsync(
        wgpu::MapMode::Read, 0, kSize,
        [](WGPUBufferMapAsyncStatus status, void* userdata) {
            EXPECT_EQ(WGPUBufferMapAsyncStatus_Success, status);
            // fence.OnCompletion callback should have set onCompleteDone to true
            EXPECT_EQ(static_cast<Done*>(userdata)->onCompleteDone, true);
            static_cast<Done*>(userdata)->mapDone = true;
        },
        &done);

    fence.OnCompletion(
        2u,
        [](WGPUFenceCompletionStatus status, void* userdata) {
            EXPECT_EQ(WGPUFenceCompletionStatus_Success, status);
            // buffer.MapAsync callback should not have ran yet, mapDone should still be false.
            EXPECT_EQ(static_cast<Done*>(userdata)->mapDone, false);
            static_cast<Done*>(userdata)->onCompleteDone = true;
        },
        &done);
    while (!done.mapDone) {
        WaitABit();
    }
    EXPECT_EQ(myData, *static_cast<const uint32_t*>(buffer.GetConstMappedRange()));
    buffer.Unmap();
}

// Test that fence.OnCompletion callback happens before buffer.MapAsync callback when
// queue.Signal is called before buffer.MapAsync. The callback order should
// happen in the order the functions are called
TEST_P(QueueTimelineTests, SignalOnCompleteMapRead) {
    uint32_t myData = 0x01020304;
    constexpr size_t kSize = sizeof(myData);
    wgpu::Buffer buffer = CreateAndWriteToBuffer(myData, kSize);

    wgpu::Fence fence = queue.CreateFence();
    queue.Signal(fence, 2);

    Done done;

    fence.OnCompletion(
        2u,
        [](WGPUFenceCompletionStatus status, void* userdata) {
            EXPECT_EQ(WGPUFenceCompletionStatus_Success, status);
            // buffer.MapAsync callback should not have ran yet, mapDone should still be false.
            EXPECT_EQ(static_cast<Done*>(userdata)->mapDone, false);
            static_cast<Done*>(userdata)->onCompleteDone = true;
        },
        &done);

    buffer.MapAsync(
        wgpu::MapMode::Read, 0, kSize,
        [](WGPUBufferMapAsyncStatus status, void* userdata) {
            EXPECT_EQ(WGPUBufferMapAsyncStatus_Success, status);
            // fence.OnCompletion callback should have set onCompleteDone to true.
            EXPECT_EQ(static_cast<Done*>(userdata)->onCompleteDone, true);
            static_cast<Done*>(userdata)->mapDone = true;
        },
        &done);
    while (!done.mapDone) {
        WaitABit();
    }
    EXPECT_EQ(myData, *static_cast<const uint32_t*>(buffer.GetConstMappedRange()));
    buffer.Unmap();
}

DAWN_INSTANTIATE_TEST(QueueTimelineTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      VulkanBackend());
