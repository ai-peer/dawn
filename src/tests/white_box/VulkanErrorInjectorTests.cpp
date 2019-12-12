// Copyright 2019 The Dawn Authors
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

#include "common/Math.h"
#include "tests/DawnTest.h"

#include "dawn_native/ErrorData.h"

#include "common/vulkan_platform.h"
#include "dawn_native/VulkanBackend.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/VulkanError.h"
#include "dawn_native/vulkan/VulkanErrorInjector.h"
// #include "utils/SystemUtils.h"
// #include "utils/WGPUHelpers.h"

namespace {

    class VulkanErrorInjectorTests : public DawnTest {
      public:
        void TestSetUp() override {
            DAWN_SKIP_TEST_IF(UsesWire());

            mDeviceVk = reinterpret_cast<dawn_native::vulkan::Device*>(device.Get());
            mErrorInjector = std::make_unique<dawn_native::vulkan::VulkanErrorInjector>();
        }

      protected:
        dawn_native::vulkan::Device* mDeviceVk;
        std::unique_ptr<dawn_native::vulkan::VulkanErrorInjector> mErrorInjector;
    };

}  // anonymous namespace

TEST_P(VulkanErrorInjectorTests, InjectErrorOnCreateBuffer) {
    VkBufferCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.size = 16;
    createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    // Check that making a buffer works.
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        EXPECT_EQ(
            mDeviceVk->fn.CreateBuffer(mDeviceVk->GetVkDevice(), &createInfo, nullptr, &buffer),
            VK_SUCCESS);
        mDeviceVk->fn.DestroyBuffer(mDeviceVk->GetVkDevice(), buffer, nullptr);
    }

    auto CreateTestBuffer = [&]() -> dawn_native::ResultOrError<VkBuffer> {
        VkBuffer buffer = VK_NULL_HANDLE;
        dawn_native::MaybeError err = CheckVkSuccess(
            mDeviceVk->fn.CreateBuffer(mDeviceVk->GetVkDevice(), &createInfo, nullptr, &buffer),
            "vkCreateBuffer");
        if (err.IsError()) {
            // The handle should never be written to, even for mock failures.
            EXPECT_EQ(buffer, VK_NULL_HANDLE);
            DAWN_TRY(std::move(err));
        }
        EXPECT_NE(buffer, VK_NULL_HANDLE);

        // We never use the buffer, only test mocking errors on creation. Cleanup now.
        mDeviceVk->fn.DestroyBuffer(mDeviceVk->GetVkDevice(), buffer, nullptr);
        return buffer;
    };

    // Check that making a buffer inside CheckVkSuccess works.
    {
        EXPECT_TRUE(CreateTestBuffer().IsSuccess());

        // The error injector call log should be empty
        EXPECT_TRUE(mErrorInjector->AcquireCallLog().empty());
    }

    // Test error injection works.
    dawn_native::vulkan::VulkanErrorInjector::Set(mErrorInjector.get());
    {
        EXPECT_TRUE(CreateTestBuffer().IsSuccess());
        EXPECT_TRUE(CreateTestBuffer().IsSuccess());

        std::unordered_map<size_t, uint64_t> callLog = mErrorInjector->AcquireCallLog();

        // The error injector call log should have one entry with two calls.
        EXPECT_EQ(callLog.size(), 1u);
        EXPECT_EQ(callLog.begin()->second, 2u);

        // Inject an error at index 0. The first should fail, the second succeed.
        {
            mErrorInjector->InjectErrorAt(callLog.begin()->first, 0u);
            EXPECT_TRUE(CreateTestBuffer().IsError());
            EXPECT_TRUE(CreateTestBuffer().IsSuccess());

            mErrorInjector->Clear();
        }

        // Inject an error at index 1. The second should fail, the first succeed.
        {
            mErrorInjector->InjectErrorAt(callLog.begin()->first, 1u);
            EXPECT_TRUE(CreateTestBuffer().IsSuccess());
            EXPECT_TRUE(CreateTestBuffer().IsError());

            mErrorInjector->Clear();
        }

        // Inject an error and then clear the injector. Calls should be successful.
        {
            mErrorInjector->InjectErrorAt(callLog.begin()->first, 0u);
            dawn_native::vulkan::VulkanErrorInjector::Set(nullptr);

            EXPECT_TRUE(CreateTestBuffer().IsSuccess());
            EXPECT_TRUE(CreateTestBuffer().IsSuccess());

            mErrorInjector->Clear();
        }
    }
}

DAWN_INSTANTIATE_TEST(VulkanErrorInjectorTests, VulkanBackend);
