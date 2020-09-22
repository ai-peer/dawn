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
    void WaitForCompletedValue(wgpu::Fence fence, uint64_t completedValue) {
        while (fence.GetCompletedValue() < completedValue) {
            WaitABit();
        }
    }

    wgpu::Buffer CreateBuffer(uint64_t size, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = size;
        descriptor.usage = usage;
        return device.CreateBuffer(&descriptor);
    }
};

// Test that Buffer.MapAsync read followed by Queue.Signal should happen in order
TEST_P(QueueTimelineTests, MapReadSignalOnComplete) {
    wgpu::Buffer buffer = CreateBuffer(4, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);
    uint32_t myData = 0x01020304;
    constexpr size_t kSize = sizeof(myData);
    queue.WriteBuffer(buffer, 0, &myData, kSize);

    // bool done = false;
    struct Done {
        bool onCompleteDone = false;
        bool mapDone = false;
    };
    Done done;

    buffer.MapAsync(
        wgpu::MapMode::Read, 0, kSize,
        [](WGPUBufferMapAsyncStatus status, void* userdata) {
            ASSERT_EQ(WGPUBufferMapAsyncStatus_Success, status);
            static_cast<Done*>(userdata)->mapDone = true;
        },
        &done);

    wgpu::Fence fence = queue.CreateFence();

    queue.Signal(fence, 1);
    fence.OnCompletion(
        1u,
        [](WGPUFenceCompletionStatus status, void* userdata) {
            // the done flag should be true after it's set by the map async callback
            EXPECT_EQ(static_cast<Done*>(userdata)->mapDone, true);
            static_cast<Done*>(userdata)->onCompleteDone = true;
        },
        &done);
    WaitForCompletedValue(fence, 1);
    while (!done.onCompleteDone) {
        WaitABit();
    }
    ASSERT_EQ(myData, *static_cast<const uint32_t*>(buffer.GetConstMappedRange()));
    buffer.Unmap();
}

// // Test that Buffer.MapAsync read followed by Queue.Signal should happen in order
// TEST_P(QueueTimelineTests, SignalMapReadOnComplete) {
//     wgpu::Fence fence = queue.CreateFence();
//     queue.Signal(fence, 2);

//     wgpu::Buffer buffer = CreateBuffer(4, wgpu::BufferUsage::MapRead |
//     wgpu::BufferUsage::CopyDst); uint32_t myData = 0x01020304; constexpr size_t kSize =
//     sizeof(myData); queue.WriteBuffer(buffer, 0, &myData, kSize);

//     bool done = false;
//     buffer.MapAsync(
//         wgpu::MapMode::Read, 0, kSize,
//         [](WGPUBufferMapAsyncStatus status, void* userdata) {
//             ASSERT_EQ(WGPUBufferMapAsyncStatus_Success, status);
//             *static_cast<bool*>(userdata) = true;
//         },
//         &done);

//     fence.OnCompletion(
//         2u,
//         [](WGPUFenceCompletionStatus status, void* userdata) {
//             // the done flag should be true since it's set by the map async callback
//             EXPECT_EQ(*static_cast<bool*>(userdata), true);
//         },
//         &done);
//     WaitForCompletedValue(fence, 2);
// }

// // Test that signal, fence on completion, map happen in order
// TEST_P(QueueTimelineTests, SignalOnCompleteMapRead) {
//     wgpu::Fence fence = queue.CreateFence();
//     queue.Signal(fence, 2);

//     wgpu::Buffer buffer = CreateBuffer(4, wgpu::BufferUsage::MapRead |
//     wgpu::BufferUsage::CopyDst); uint32_t myData = 0x01020304; constexpr size_t kSize =
//     sizeof(myData); queue.WriteBuffer(buffer, 0, &myData, kSize);

//     struct Done {
//         bool onCompleteDone = false;
//         bool mapDone = false;
//     };
//     Done done;

//     fence.OnCompletion(
//         2u,
//         [](WGPUFenceCompletionStatus status, void* userdata) {
//             // the done flag should be true since it's set by the map async callback
//             static_cast<Done*>(userdata)->onCompleteDone = true;
//         },
//         &done);
//     WaitForCompletedValue(fence, 2);

//     buffer.MapAsync(
//         wgpu::MapMode::Read, 0, kSize,
//         [](WGPUBufferMapAsyncStatus status, void* userdata) {
//             ASSERT_EQ(WGPUBufferMapAsyncStatus_Success, status);
//             EXPECT_EQ(static_cast<Done*>(userdata)->onCompleteDone, true);
//             static_cast<Done*>(userdata)->mapDone = true;
//         },
//         &done);
//     while (!done.mapDone) {
//         WaitABit();
//     }
// }

DAWN_INSTANTIATE_TEST(QueueTimelineTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      VulkanBackend());
