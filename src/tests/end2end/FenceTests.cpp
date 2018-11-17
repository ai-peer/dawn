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

#include "tests/DawnTest.h"

#include <array>
#include <cstring>

class FenceTests : public DawnTest {
  private:
    struct CallbackInfo {
        FenceTests* test;
        uint64_t value;
        dawnFenceCompletionStatus status;
        int32_t callIndex = -1; // If this is -1, the callback was not called

        void Update(dawnFenceCompletionStatus status) {
            this->callIndex = test->mCallIndex++;
            this->status = status;
        }
    };

    int32_t mCallIndex;

  protected:
    FenceTests() : mCallIndex(0) {
    }

    static void OnCompletionCallback(dawnFenceCompletionStatus status, dawn::CallbackUserdata userdata) {
        auto* callback = reinterpret_cast<CallbackInfo*>(userdata);
        callback->Update(status);
    }

    CallbackInfo* TestOnCompletionCallback(dawn::Fence fence, uint64_t value) {
        auto* callback = new CallbackInfo;
        callback->test = this;
        callback->value = value;
        fence.OnCompletion(value, OnCompletionCallback, reinterpret_cast<uintptr_t>(callback));
        return callback;
    }

    void WaitForCompletedValue(dawn::Fence fence, uint64_t completedValue) {
        while (fence.GetCompletedValue() < completedValue) {
            WaitABit();
        }
    }
};

// Test that signaling a fence updates the completed value
TEST_P(FenceTests, SimpleSignal) {
    dawn::FenceDescriptor descriptor;
    descriptor.initialValue = 1u;
    dawn::Fence fence = device.CreateFence(&descriptor);

    // Completed value starts at initial value
    EXPECT_EQ(fence.GetCompletedValue(), 1u);

    queue.Signal(fence, 2);
    WaitForCompletedValue(fence, 2);

    // Completed value updates to signaled value
    EXPECT_EQ(fence.GetCompletedValue(), 2u);
}

// Test callbacks are called in increasing order of fence completion value
TEST_P(FenceTests, OnCompletionOrdering) {
    dawn::FenceDescriptor descriptor;
    descriptor.initialValue = 0u;
    dawn::Fence fence = device.CreateFence(&descriptor);

    queue.Signal(fence, 4);

    auto* callback2 = TestOnCompletionCallback(fence, 2u);
    auto* callback0 = TestOnCompletionCallback(fence, 0u);
    auto* callback3 = TestOnCompletionCallback(fence, 3u);
    auto* callback1 = TestOnCompletionCallback(fence, 1u);

    WaitForCompletedValue(fence, 4);

    EXPECT_EQ(callback0->callIndex, 0);
    EXPECT_EQ(callback1->callIndex, 1);
    EXPECT_EQ(callback2->callIndex, 2);
    EXPECT_EQ(callback3->callIndex, 3);
    EXPECT_EQ(callback0->status, DAWN_FENCE_COMPLETION_STATUS_SUCCESS);
    EXPECT_EQ(callback1->status, DAWN_FENCE_COMPLETION_STATUS_SUCCESS);
    EXPECT_EQ(callback2->status, DAWN_FENCE_COMPLETION_STATUS_SUCCESS);
    EXPECT_EQ(callback3->status, DAWN_FENCE_COMPLETION_STATUS_SUCCESS);

    delete callback0;
    delete callback1;
    delete callback2;
    delete callback3;
}

// Test callbacks still occur if Queue::Signal happens multiple times
TEST_P(FenceTests, MultipleSignalOnCompletion) {
    dawn::FenceDescriptor descriptor;
    descriptor.initialValue = 0u;
    dawn::Fence fence = device.CreateFence(&descriptor);

    queue.Signal(fence, 2);
    queue.Signal(fence, 4);

    auto* callback = TestOnCompletionCallback(fence, 3u);

    WaitForCompletedValue(fence, 4);

    EXPECT_EQ(callback->callIndex, 0);
    EXPECT_EQ(callback->status, DAWN_FENCE_COMPLETION_STATUS_SUCCESS);
    delete callback;
}


// Test all callbacks are called if they are added for the same fence value
TEST_P(FenceTests, OnCompletionMultipleCallbacks) {
    dawn::FenceDescriptor descriptor;
    descriptor.initialValue = 0u;
    dawn::Fence fence = device.CreateFence(&descriptor);

    queue.Signal(fence, 4);

    auto* callback0 = TestOnCompletionCallback(fence, 4u);
    auto* callback1 = TestOnCompletionCallback(fence, 4u);
    auto* callback2 = TestOnCompletionCallback(fence, 4u);
    auto* callback3 = TestOnCompletionCallback(fence, 4u);

    while (fence.GetCompletedValue() < 4u) {
        WaitABit();
    }

    EXPECT_GE(callback0->callIndex, 0);
    EXPECT_GE(callback1->callIndex, 0);
    EXPECT_GE(callback2->callIndex, 0);
    EXPECT_GE(callback3->callIndex, 0);
    EXPECT_EQ(callback0->status, DAWN_FENCE_COMPLETION_STATUS_SUCCESS);
    EXPECT_EQ(callback1->status, DAWN_FENCE_COMPLETION_STATUS_SUCCESS);
    EXPECT_EQ(callback2->status, DAWN_FENCE_COMPLETION_STATUS_SUCCESS);
    EXPECT_EQ(callback3->status, DAWN_FENCE_COMPLETION_STATUS_SUCCESS);

    delete callback0;
    delete callback1;
    delete callback2;
    delete callback3;
}

DAWN_INSTANTIATE_TEST(FenceTests, D3D12Backend, MetalBackend, OpenGLBackend, VulkanBackend)
