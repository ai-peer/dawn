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

#include "tests/unittests/validation/ValidationTest.h"

#include <gmock/gmock.h>

using namespace testing;

class MockFenceOnCompletionCallback {
  public:
    MOCK_METHOD2(Call, void(dawnFenceCompletionStatus status, dawnCallbackUserdata userdata));
};

struct FenceOnCompletionExpectation {
    dawn::Fence fence;
    uint64_t value;
    dawnFenceCompletionStatus status;
};

static std::unique_ptr<MockFenceOnCompletionCallback> mockFenceOnCompletionCallback;
static void ToMockFenceOnCompletionCallback(dawnFenceCompletionStatus status,
                                            dawnCallbackUserdata userdata) {
    mockFenceOnCompletionCallback->Call(status, userdata);
    auto* data = reinterpret_cast<FenceOnCompletionExpectation*>(static_cast<uintptr_t>(userdata));
    if (status == DAWN_FENCE_COMPLETION_STATUS_SUCCESS) {
        EXPECT_GE(data->fence.GetCompletedValue(), data->value);
    }
    delete data;
}

class FenceValidationTest : public ValidationTest {
  protected:
    void TestOnCompletion(dawn::Fence fence, uint64_t value, dawnFenceCompletionStatus status) {
        FenceOnCompletionExpectation* expectation = new FenceOnCompletionExpectation;
        expectation->fence = fence;
        expectation->value = value;
        expectation->status = status;
        dawnCallbackUserdata userdata =
            static_cast<dawnCallbackUserdata>(reinterpret_cast<uintptr_t>(expectation));

        EXPECT_CALL(*mockFenceOnCompletionCallback, Call(status, userdata)).Times(1);
        fence.OnCompletion(value, ToMockFenceOnCompletionCallback, userdata);
    }

    dawn::Queue queue;

  private:
    void SetUp() override {
        ValidationTest::SetUp();

        mockFenceOnCompletionCallback = std::make_unique<MockFenceOnCompletionCallback>();
        queue = device.CreateQueue();
    }

    void TearDown() override {
        // Delete mocks so that expectations are checked
        mockFenceOnCompletionCallback = nullptr;

        ValidationTest::TearDown();
    }
};

// Test cases where creation should succeed
TEST_F(FenceValidationTest, CreationSuccess) {
    // Success
    {
        dawn::FenceDescriptor descriptor;
        descriptor.initialValue = 0;
        device.CreateFence(&descriptor);
    }
}

TEST_F(FenceValidationTest, GetCompletedValue) {
    // Starts at initial value
    {
        dawn::FenceDescriptor descriptor;
        descriptor.initialValue = 1;
        dawn::Fence fence = device.CreateFence(&descriptor);
        EXPECT_EQ(fence.GetCompletedValue(), 1u);
    }
}

TEST_F(FenceValidationTest, OnCompletion) {
    dawn::FenceDescriptor descriptor;
    descriptor.initialValue = 1;
    dawn::Fence fence = device.CreateFence(&descriptor);

    // Can call on values <= (initial) signaled value
    TestOnCompletion(fence, 0, DAWN_FENCE_COMPLETION_STATUS_SUCCESS);
    TestOnCompletion(fence, 1, DAWN_FENCE_COMPLETION_STATUS_SUCCESS);

    // Cannot call on values > signaled value
    ASSERT_DEVICE_ERROR(TestOnCompletion(fence, 2, DAWN_FENCE_COMPLETION_STATUS_ERROR));

    // // Can call after signaling
    queue.Signal(fence, 2);
    TestOnCompletion(fence, 2, DAWN_FENCE_COMPLETION_STATUS_SUCCESS);

    // Flush
    queue.Submit(0, nullptr);
}

TEST_F(FenceValidationTest, Signal) {
    dawn::FenceDescriptor descriptor;
    descriptor.initialValue = 1;
    dawn::Fence fence = device.CreateFence(&descriptor);

    // value < fence signaled value
    ASSERT_DEVICE_ERROR(queue.Signal(fence, 0));

    // value == fence signaled value
    ASSERT_DEVICE_ERROR(queue.Signal(fence, 1));

    // Success
    queue.Signal(fence, 2);
    queue.Submit(0, nullptr);
    EXPECT_EQ(fence.GetCompletedValue(), 2u);

    // Success increasing fence value by more than 1
    queue.Signal(fence, 6);
    queue.Submit(0, nullptr);
    EXPECT_EQ(fence.GetCompletedValue(), 6u);
}