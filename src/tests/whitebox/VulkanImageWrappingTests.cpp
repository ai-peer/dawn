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

#include "tests/DawnTest.h"

#include "common/vulkan_platform.h"
#include "dawn_native/VulkanBackend.h"
#include "dawn_native/vulkan/AdapterVk.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/FencedDeleter.h"
#include "dawn_native/vulkan/MemoryAllocator.h"
#include "dawn_native/vulkan/TextureVk.h"
#include "utils/DawnHelpers.h"
#include "utils/SystemUtils.h"

namespace {

    class VulkanImageWrappingTestBase : public DawnTest {
      public:
        void SetUp() override {
            DawnTest::SetUp();
            if (UsesWire()) {
                return;
            }

            deviceVk = reinterpret_cast<dawn_native::vulkan::Device*>(device.Get());
        }

        // Creates a VkImage with external memory
        VkResult CreateImage(dawn_native::vulkan::Device* deviceVk,
                             uint32_t width,
                             uint32_t height,
                             VkFormat format,
                             VkImage* image) {
            VkExternalMemoryImageCreateInfoKHR externalInfo;
            externalInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR;
            externalInfo.pNext = nullptr;
            externalInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

            auto usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT;

            VkImageCreateInfo createInfo;
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            createInfo.pNext = &externalInfo;
            createInfo.flags = 0;
            createInfo.imageType = VK_IMAGE_TYPE_2D;
            createInfo.format = format;
            createInfo.extent = {width, height, 1};
            createInfo.mipLevels = 1;
            createInfo.arrayLayers = 1;
            createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            createInfo.usage = usage;
            createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
            createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            return deviceVk->fn.CreateImage(deviceVk->GetVkDevice(), &createInfo, nullptr, image);
        }

        // Allocates memory for an image
        bool AllocateMemory(dawn_native::vulkan::Device* deviceVk,
                            VkImage handle,
                            dawn_native::vulkan::DeviceMemoryAllocation* memory) {
            // Create the image memory and associate it with the container
            VkMemoryRequirements requirements;
            deviceVk->fn.GetImageMemoryRequirements(deviceVk->GetVkDevice(), handle, &requirements);

            // Import memory from file descriptor
            VkExportMemoryAllocateInfoKHR externalInfo;
            externalInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR;
            externalInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

            return deviceVk->GetMemoryAllocator()->Allocate(requirements, false, &externalInfo,
                                                            memory);
        }

        // Binds memory to an image
        VkResult BindMemory(dawn_native::vulkan::Device* deviceVk,
                            VkImage handle,
                            dawn_native::vulkan::DeviceMemoryAllocation* memory) {
            return deviceVk->fn.BindImageMemory(deviceVk->GetVkDevice(), handle,
                                                memory->GetMemory(), memory->GetMemoryOffset());
        }

        // Extracts a file descriptor representing memory on a device
        int GetMemoryFd(dawn_native::vulkan::Device* deviceVk, VkDeviceMemory memory) {
            VkMemoryGetFdInfoKHR getFdInfo;
            getFdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
            getFdInfo.pNext = nullptr;
            getFdInfo.memory = memory;
            getFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

            int memoryFd = -1;
            deviceVk->fn.GetMemoryFdKHR(deviceVk->GetVkDevice(), &getFdInfo, &memoryFd);

            EXPECT_GE(memoryFd, 0) << "Failed to get file descriptor for external memory";
            return memoryFd;
        }

        // Prepares and exports memory for an image on a given device
        void CreateBindExportImage(dawn_native::vulkan::Device* deviceVk,
                                   uint32_t width,
                                   uint32_t height,
                                   VkFormat format,
                                   VkImage* handle,
                                   dawn_native::vulkan::DeviceMemoryAllocation* allocation,
                                   int* memoryFd) {
            VkResult result = CreateImage(deviceVk, width, height, format, handle);
            EXPECT_EQ(result, VK_SUCCESS) << "Failed to create external image";

            bool resultBool = AllocateMemory(deviceVk, *handle, allocation);
            EXPECT_EQ(resultBool, true) << "Failed to allocate external memory";

            result = BindMemory(deviceVk, *handle, allocation);
            EXPECT_EQ(result, VK_SUCCESS) << "Failed to bind image memory";

            *memoryFd = GetMemoryFd(deviceVk, allocation->GetMemory());
        }

        // Wraps a vulkan image from external memory
        dawn::Texture WrapVulkanImage(dawn::Device device,
                                      const dawn::TextureDescriptor* descriptor,
                                      int memoryFd,
                                      std::vector<int> waitFds) {
            DawnTexture texture = dawn_native::vulkan::WrapVulkanImage(
                device.Get(), reinterpret_cast<const DawnTextureDescriptor*>(descriptor), memoryFd,
                waitFds);
            return dawn::Texture::Acquire(texture);
        }

      protected:
        dawn_native::vulkan::Device* deviceVk;
    };

}  // anonymous namespace

// Fixture to test using external memory textures through different usages.
// These tests are skipped if the harness is using the wire.
class VulkanImageWrappingUsageTests : public VulkanImageWrappingTestBase {
  public:
    void SetUp() override {
        VulkanImageWrappingTestBase::SetUp();
        if (UsesWire()) {
            return;
        }

        // Create another device based on the original
        backendAdapter = reinterpret_cast<dawn_native::vulkan::Adapter*>(deviceVk->GetAdapter());
        deviceDescriptor.forceEnabledToggles = GetParam().forceEnabledWorkarounds;
        deviceDescriptor.forceDisabledToggles = GetParam().forceDisabledWorkarounds;

        secondDeviceVk = reinterpret_cast<dawn_native::vulkan::Device*>(
            backendAdapter->CreateDevice(&deviceDescriptor));
        secondDevice = dawn::Device::Acquire(reinterpret_cast<DawnDevice>(secondDeviceVk));

        CreateBindExportImage(deviceVk, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, &defaultImage,
                              &defaultAllocation, &defaultFd);
        defaultDescriptor.dimension = dawn::TextureDimension::e2D;
        defaultDescriptor.format = dawn::TextureFormat::RGBA8Unorm;
        defaultDescriptor.size = {1, 1, 1};
        defaultDescriptor.sampleCount = 1;
        defaultDescriptor.arrayLayerCount = 1;
        defaultDescriptor.mipLevelCount = 1;
        defaultDescriptor.usage = dawn::TextureUsageBit::OutputAttachment |
                                  dawn::TextureUsageBit::CopySrc | dawn::TextureUsageBit::CopyDst;
    }

    void TearDown() override {
        if (UsesWire()) {
            VulkanImageWrappingTestBase::TearDown();
            return;
        }

        deviceVk->GetFencedDeleter()->DeleteWhenUnused(defaultImage);
        deviceVk->GetMemoryAllocator()->Free(&defaultAllocation);
        VulkanImageWrappingTestBase::TearDown();
    }

  protected:
    dawn::Device secondDevice;
    dawn_native::vulkan::Device* secondDeviceVk;

    dawn_native::vulkan::Adapter* backendAdapter;
    dawn_native::DeviceDescriptor deviceDescriptor;

    dawn::TextureDescriptor defaultDescriptor;
    VkImage defaultImage;
    dawn_native::vulkan::DeviceMemoryAllocation defaultAllocation;
    int defaultFd;

    // Clear a texture on a given device
    void ClearImage(dawn::Device device, dawn::Texture wrappedTexture, dawn::Color clearColor) {
        dawn::TextureView wrappedView = wrappedTexture.CreateDefaultView();

        // Submit a clear operation
        utils::ComboRenderPassDescriptor renderPassDescriptor({wrappedView}, {});
        renderPassDescriptor.cColorAttachmentsInfoPtr[0]->clearColor = clearColor;

        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDescriptor);
        pass.EndPass();

        dawn::CommandBuffer commands = encoder.Finish();

        dawn::Queue queue = device.CreateQueue();
        queue.Submit(1, &commands);
    }

    // Submits a 1x1x1 copy from source to destination
    void SimpleCopyTextureToTexture(dawn::Device device,
                                    dawn::Queue queue,
                                    dawn::Texture source,
                                    dawn::Texture destination) {
        dawn::TextureCopyView copySrc;
        copySrc.texture = source;
        copySrc.mipLevel = 0;
        copySrc.arrayLayer = 0;
        copySrc.origin = {0, 0, 0};

        dawn::TextureCopyView copyDst;
        copyDst.texture = destination;
        copyDst.mipLevel = 0;
        copyDst.arrayLayer = 0;
        copyDst.origin = {0, 0, 0};

        dawn::Extent3D copySize = {1, 1, 1};

        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyTextureToTexture(&copySrc, &copyDst, &copySize);
        dawn::CommandBuffer commands = encoder.Finish();

        queue.Submit(1, &commands);
    }
};

// Clear an image in |secondDevice|
// Verify clear color is visible in |device|
TEST_P(VulkanImageWrappingUsageTests, ClearImageAcrossDevices) {
    DAWN_SKIP_TEST_IF(UsesWire());

    // Import the image on |secondDevice|
    dawn::Texture wrappedTexture = WrapVulkanImage(secondDevice, &defaultDescriptor, defaultFd, {});
    int signalFd =
        dawn_native::vulkan::ExportSignalSemaphoreFd(secondDevice.Get(), wrappedTexture.Get());

    // Import the image to |device|, making sure we wait on signalFd
    int memoryFd = GetMemoryFd(deviceVk, defaultAllocation.GetMemory());
    dawn::Texture nextWrappedTexture =
        WrapVulkanImage(device, &defaultDescriptor, memoryFd, {signalFd});

    // Clear |wrappedTexture| on |secondDevice|
    ClearImage(secondDevice, wrappedTexture, {1 / 255.0f, 2 / 255.0f, 3 / 255.0f, 4 / 255.0f});

    // Verify |device| sees the changes from |secondDevice|
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(1, 2, 3, 4), nextWrappedTexture, 0, 0);
}

// Import a texture from |secondDevice|
// Issue a copy of the imported texture inside |device| to |copyDstTexture|
// Verify the clear color from |secondDevice| is visible in |copyDstTexture|
TEST_P(VulkanImageWrappingUsageTests, CopyTextureToTextureSrcSync) {
    DAWN_SKIP_TEST_IF(UsesWire());

    // Import the image on |secondDevice|
    dawn::Texture wrappedTexture = WrapVulkanImage(secondDevice, &defaultDescriptor, defaultFd, {});
    int signalFd =
        dawn_native::vulkan::ExportSignalSemaphoreFd(secondDevice.Get(), wrappedTexture.Get());

    // Import the image to |device|, making sure we wait on |signalFd|
    int memoryFd = GetMemoryFd(deviceVk, defaultAllocation.GetMemory());
    dawn::Texture deviceWrappedTexture =
        WrapVulkanImage(device, &defaultDescriptor, memoryFd, {signalFd});

    // Create a second texture on |device|
    dawn::Texture copyDstTexture = device.CreateTexture(&defaultDescriptor);

    // Copy |deviceWrappedTexture| into |copyDstTexture|
    SimpleCopyTextureToTexture(device, queue, deviceWrappedTexture, copyDstTexture);

    // After submitting the operations on |device|, clear |wrappedTexture| on |secondDevice|
    ClearImage(secondDevice, wrappedTexture, {1 / 255.0f, 2 / 255.0f, 3 / 255.0f, 4 / 255.0f});

    // Verify |copyDstTexture| sees changes from |secondDevice|
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(1, 2, 3, 4), copyDstTexture, 0, 0);
}

// Import a texture from |secondDevice|
// Issue a copy of color A into the imported texture inside |device| to |deviceWrappedTexture|
// Clear the texture with color B in |secondDevice|
// Verify the texture contains color A
TEST_P(VulkanImageWrappingUsageTests, CopyTextureToTextureDstSync) {
    DAWN_SKIP_TEST_IF(UsesWire());

    // Import the image on |secondDevice|
    dawn::Texture wrappedTexture = WrapVulkanImage(secondDevice, &defaultDescriptor, defaultFd, {});
    int signalFd =
        dawn_native::vulkan::ExportSignalSemaphoreFd(secondDevice.Get(), wrappedTexture.Get());

    // Import the image to |device|, making sure we wait on |signalFd|
    int memoryFd = GetMemoryFd(deviceVk, defaultAllocation.GetMemory());
    dawn::Texture deviceWrappedTexture =
        WrapVulkanImage(device, &defaultDescriptor, memoryFd, {signalFd});

    // Create a second texture on |device|
    dawn::Texture copySrcTexture = device.CreateTexture(&defaultDescriptor);
    ClearImage(device, copySrcTexture, {1 / 255.0f, 2 / 255.0f, 3 / 255.0f, 4 / 255.0f});

    // Copy |copySrcTexture| into |deviceWrappedTexture|
    SimpleCopyTextureToTexture(device, queue, copySrcTexture, deviceWrappedTexture);

    // After submitting the operations on |device|, clear |wrappedTexture| on |secondDevice|
    ClearImage(secondDevice, wrappedTexture, {5 / 255.0f, 6 / 255.0f, 7 / 255.0f, 8 / 255.0f});

    // Verify |deviceWrappedTexture| contains the color from our copy
    // (since it should wait until |secondDevice| is done to do this copy)
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(1, 2, 3, 4), deviceWrappedTexture, 0, 0);
}

// Import a texture from |secondDevice|
// Issue a copy of the imported texture inside |device| to |copyDstBuffer|
// Verify the clear color from |secondDevice| is visible in |copyDstBuffer|
TEST_P(VulkanImageWrappingUsageTests, CopyTextureToBufferSrcSync) {
    DAWN_SKIP_TEST_IF(UsesWire());

    // Import the image on |secondDevice|
    dawn::Texture wrappedTexture = WrapVulkanImage(secondDevice, &defaultDescriptor, defaultFd, {});
    int signalFd =
        dawn_native::vulkan::ExportSignalSemaphoreFd(secondDevice.Get(), wrappedTexture.Get());

    // Import the image to |device|, making sure we wait on |signalFd|
    int memoryFd = GetMemoryFd(deviceVk, defaultAllocation.GetMemory());
    dawn::Texture deviceWrappedTexture =
        WrapVulkanImage(device, &defaultDescriptor, memoryFd, {signalFd});

    // Create a destination buffer on |device|
    dawn::BufferDescriptor bufferDesc;
    bufferDesc.size = 4;
    bufferDesc.usage = dawn::BufferUsageBit::CopyDst | dawn::BufferUsageBit::CopySrc;
    dawn::Buffer copyDstBuffer = device.CreateBuffer(&bufferDesc);

    // Copy |deviceWrappedTexture| into |copyDstBuffer|
    dawn::TextureCopyView copySrc;
    copySrc.texture = deviceWrappedTexture;
    copySrc.mipLevel = 0;
    copySrc.arrayLayer = 0;
    copySrc.origin = {0, 0, 0};

    dawn::BufferCopyView copyDst;
    copyDst.buffer = copyDstBuffer;
    copyDst.offset = 0;
    copyDst.rowPitch = 256;
    copyDst.imageHeight = 0;

    dawn::Extent3D copySize = {1, 1, 1};

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToBuffer(&copySrc, &copyDst, &copySize);
    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    // After submitting the operations on |device|, clear |wrappedTexture| on |secondDevice|
    ClearImage(secondDevice, wrappedTexture, {1 / 255.0f, 2 / 255.0f, 3 / 255.0f, 4 / 255.0f});

    // Verify |copyDstBuffer| sees changes from |secondDevice|
    uint32_t expected = 0x04030201;
    EXPECT_BUFFER_U32_EQ(expected, copyDstBuffer, 0);
}

// Import a texture from |secondDevice|
// Issue a copy of color A into the imported texture inside |device| to |deviceWrappedTexture|
// Clear the texture with color B in |secondDevice|
// Verify the texture contains color A
TEST_P(VulkanImageWrappingUsageTests, CopyBufferToTextureDstSync) {
    DAWN_SKIP_TEST_IF(UsesWire());

    // Import the image on |secondDevice|
    dawn::Texture wrappedTexture = WrapVulkanImage(secondDevice, &defaultDescriptor, defaultFd, {});
    int signalFd =
        dawn_native::vulkan::ExportSignalSemaphoreFd(secondDevice.Get(), wrappedTexture.Get());

    // Import the image to |device|, making sure we wait on |signalFd|
    int memoryFd = GetMemoryFd(deviceVk, defaultAllocation.GetMemory());
    dawn::Texture deviceWrappedTexture =
        WrapVulkanImage(device, &defaultDescriptor, memoryFd, {signalFd});

    // Create a buffer on |device|
    dawn::Buffer copySrcBuffer =
        utils::CreateBufferFromData(device, dawn::BufferUsageBit::CopySrc, {0x04030201});

    // Copy |deviceWrappedTexture| into |copyDstBuffer|
    dawn::BufferCopyView copySrc;
    copySrc.buffer = copySrcBuffer;
    copySrc.offset = 0;
    copySrc.rowPitch = 256;
    copySrc.imageHeight = 0;

    dawn::TextureCopyView copyDst;
    copyDst.texture = deviceWrappedTexture;
    copyDst.mipLevel = 0;
    copyDst.arrayLayer = 0;
    copyDst.origin = {0, 0, 0};

    dawn::Extent3D copySize = {1, 1, 1};

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&copySrc, &copyDst, &copySize);
    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    // After submitting the operations on |device|, clear |wrappedTexture| on |secondDevice|
    ClearImage(secondDevice, wrappedTexture, {5 / 255.0f, 6 / 255.0f, 7 / 255.0f, 8 / 255.0f});

    // Verify |deviceWrappedTexture| contains the color from our copy
    // (since it should wait until |secondDevice| is done to do this copy)
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(1, 2, 3, 4), deviceWrappedTexture, 0, 0);
}

// Import a texture from |secondDevice|
// Issue a copy of the imported texture inside |device| to |copyDstTexture|
// Issue second copy to |secondCopyDstTexture|
// Verify the clear color from |secondDevice| is visible in both copies
TEST_P(VulkanImageWrappingUsageTests, DoubleTextureUsage) {
    DAWN_SKIP_TEST_IF(UsesWire());

    // Import the image on |secondDevice|
    dawn::Texture wrappedTexture = WrapVulkanImage(secondDevice, &defaultDescriptor, defaultFd, {});
    int signalFd =
        dawn_native::vulkan::ExportSignalSemaphoreFd(secondDevice.Get(), wrappedTexture.Get());

    // Import the image to |device|, making sure we wait on |signalFd|
    int memoryFd = GetMemoryFd(deviceVk, defaultAllocation.GetMemory());
    dawn::Texture deviceWrappedTexture =
        WrapVulkanImage(device, &defaultDescriptor, memoryFd, {signalFd});

    // Create a second texture on |device|
    dawn::Texture copyDstTexture = device.CreateTexture(&defaultDescriptor);

    // Create a third texture on |device|
    dawn::Texture secondCopyDstTexture = device.CreateTexture(&defaultDescriptor);

    // Copy |deviceWrappedTexture| into |copyDstTexture|
    SimpleCopyTextureToTexture(device, queue, deviceWrappedTexture, copyDstTexture);

    // Copy |deviceWrappedTexture| into |secondCopyDstTexture|
    SimpleCopyTextureToTexture(device, queue, deviceWrappedTexture, secondCopyDstTexture);

    // After submitting the operations on |device|, clear |wrappedTexture| on |secondDevice|
    ClearImage(secondDevice, wrappedTexture, {1 / 255.0f, 2 / 255.0f, 3 / 255.0f, 4 / 255.0f});

    // Verify |copyDstTexture| sees changes from |secondDevice|
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(1, 2, 3, 4), copyDstTexture, 0, 0);

    // Verify |secondCopyDstTexture| sees changes from |secondDevice|
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(1, 2, 3, 4), secondCopyDstTexture, 0, 0);
}

// Tex A on device 3 (external export)
// Tex B on device 2 (external export)
// Tex C on device 1 (external export)
// Queue up operations:
// Clear color for A on device 3
// Copy A->B on device 3
// Copy B->C on device 2 (wait on B from previous op)
// Copy C->D on device 1 (wait on C from previous op)
// Verify D has same color as A
// Submit the operations in any order

// Currently, self loops are now allowed (chain of copies that ends up back at
// device that started the copy). This is since we signal textures when they
// are destroyed, so the signal that starts the operations would get queued
// after the starting device is waiting for the end of the chain to finish.
// If we want to allow self loops, consider allowing textures to signal
// completion early go in to a "signaled" state. We would reject any signaled
// texture from being used again.
TEST_P(VulkanImageWrappingUsageTests, ChainTextureCopy) {
    DAWN_SKIP_TEST_IF(UsesWire());

    // Close |defaultFd| since this test doesn't import it anywhere
    close(defaultFd);

    // device 1 = |device|
    // device 2 = |secondDevice|
    // Create device 3
    dawn_native::vulkan::Device* thirdDeviceVk = reinterpret_cast<dawn_native::vulkan::Device*>(
        backendAdapter->CreateDevice(&deviceDescriptor));
    dawn::Device thirdDevice = dawn::Device::Acquire(reinterpret_cast<DawnDevice>(thirdDeviceVk));

    // Make queue for device 2 and 3
    dawn::Queue secondDeviceQueue = secondDevice.CreateQueue();
    dawn::Queue thirdDeviceQueue = thirdDevice.CreateQueue();

    std::vector<int> operationOrder = {0, 1, 2};
    int uniqueColor = 0;

    do {
        // Allocate memory for A, B, C
        VkImage imageA;
        dawn_native::vulkan::DeviceMemoryAllocation allocationA;
        int memoryFdA;
        CreateBindExportImage(thirdDeviceVk, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, &imageA, &allocationA,
                              &memoryFdA);

        VkImage imageB;
        dawn_native::vulkan::DeviceMemoryAllocation allocationB;
        int memoryFdB;
        CreateBindExportImage(secondDeviceVk, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, &imageB, &allocationB,
                              &memoryFdB);

        VkImage imageC;
        dawn_native::vulkan::DeviceMemoryAllocation allocationC;
        int memoryFdC;
        CreateBindExportImage(deviceVk, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, &imageC, &allocationC,
                              &memoryFdC);

        // Import both sides of the copy X->Y on the device it occurs

        // Import TexA, TexB on device 3
        dawn::Texture wrappedTexADevice3 =
            WrapVulkanImage(thirdDevice, &defaultDescriptor, memoryFdA, {});

        dawn::Texture wrappedTexBDevice3 =
            WrapVulkanImage(thirdDevice, &defaultDescriptor, memoryFdB, {});
        int signalFdTexBDevice3 = dawn_native::vulkan::ExportSignalSemaphoreFd(
            thirdDevice.Get(), wrappedTexBDevice3.Get());

        // Import TexB, TexC on device 2
        memoryFdB = GetMemoryFd(secondDeviceVk, allocationB.GetMemory());
        dawn::Texture wrappedTexBDevice2 =
            WrapVulkanImage(secondDevice, &defaultDescriptor, memoryFdB, {signalFdTexBDevice3});

        dawn::Texture wrappedTexCDevice2 =
            WrapVulkanImage(secondDevice, &defaultDescriptor, memoryFdC, {});
        int signalFdTexCDevice2 = dawn_native::vulkan::ExportSignalSemaphoreFd(
            secondDevice.Get(), wrappedTexCDevice2.Get());

        // Import TexC on device 1
        memoryFdC = GetMemoryFd(deviceVk, allocationC.GetMemory());
        dawn::Texture wrappedTexCDevice1 =
            WrapVulkanImage(device, &defaultDescriptor, memoryFdC, {signalFdTexCDevice2});

        // Create TexD on device 1
        dawn::Texture texD = device.CreateTexture(&defaultDescriptor);

        std::vector<std::function<void(void)>> operations = {
            [&] {
                // Clear image with current iteration as part of the color
                ClearImage(thirdDevice, wrappedTexADevice3,
                           {uniqueColor / 255.0f, 1 / 255.0f, 2 / 255.0f, 3 / 255.0f});

                // Copy A->B on device 3
                SimpleCopyTextureToTexture(thirdDevice, thirdDeviceQueue, wrappedTexADevice3,
                                           wrappedTexBDevice3);
            },
            [&] {
                // Copy B->C on device 2
                SimpleCopyTextureToTexture(secondDevice, secondDeviceQueue, wrappedTexBDevice2,
                                           wrappedTexCDevice2);
            },
            [&] {
                // Copy C->D on device 1
                SimpleCopyTextureToTexture(device, queue, wrappedTexCDevice1, texD);
            }};

        // Run operations in the order defined in |operationOrder|
        for (int i = 0; i < 3; ++i) {
            operations[operationOrder[i]]();
            utils::USleep(1000);
        }

        // Verify D matches clear color
        EXPECT_PIXEL_RGBA8_EQ(RGBA8(uniqueColor, 1, 2, 3), texD, 0, 0);

        thirdDeviceVk->GetFencedDeleter()->DeleteWhenUnused(imageA);
        thirdDeviceVk->GetMemoryAllocator()->Free(&allocationA);
        secondDeviceVk->GetFencedDeleter()->DeleteWhenUnused(imageB);
        secondDeviceVk->GetMemoryAllocator()->Free(&allocationB);
        deviceVk->GetFencedDeleter()->DeleteWhenUnused(imageC);
        deviceVk->GetMemoryAllocator()->Free(&allocationC);

        uniqueColor++;
    } while (std::next_permutation(operationOrder.begin(), operationOrder.end()));
}

DAWN_INSTANTIATE_TEST(VulkanImageWrappingUsageTests, VulkanBackend);
