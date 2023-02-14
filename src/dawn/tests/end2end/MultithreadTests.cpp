// Copyright 2023 The Dawn Authors
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

#include <condition_variable>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>

#include "dawn/common/Constants.h"
#include "dawn/common/Math.h"
#include "dawn/tests/DawnTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/TestUtils.h"
#include "dawn/utils/TextureUtils.h"
#include "dawn/utils/WGPUHelpers.h"

namespace {
template <typename Step>
class LockStep {
  public:
    LockStep() = delete;
    explicit LockStep(Step startStep) : mStep(startStep) {}

    void Signal(Step step) {
        std::lock_guard<std::mutex> lg(mMutex);
        mStep = step;
        mCv.notify_all();
    }

    void Wait(Step step) {
        std::unique_lock<std::mutex> lg(mMutex);
        mCv.wait(lg, [=] { return mStep == step; });
    }

  private:
    Step mStep;
    std::mutex mMutex;
    std::condition_variable mCv;
};

template <typename T>
class GuardedObject {
  public:
    GuardedObject& operator=(const T& obj) {
        std::lock_guard<std::mutex> lg(mMutex);
        mObj = obj;
        return *this;
    }

    T Get() const {
        std::lock_guard<std::mutex> lg(mMutex);
        return mObj;
    }

  private:
    T mObj;
    mutable std::mutex mMutex;
};

using GuardedBuffer = GuardedObject<wgpu::Buffer>;
using GuardedTexture = GuardedObject<wgpu::Texture>;
}  // namespace

class MultithreadTests : public DawnTest {
  protected:
    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        std::vector<wgpu::FeatureName> features;
        // TODO(crbug.com/dawn/1678): DawnWire doesn't support thread safe API yet.
        if (!UsesWire()) {
            features.push_back(wgpu::FeatureName::ThreadSafeAPI);
        }
        return features;
    }

    void SetUp() override {
        DawnTest::SetUp();
        // TODO(crbug.com/dawn/1678): DawnWire doesn't support thread safe API yet.
        DAWN_TEST_UNSUPPORTED_IF(UsesWire());

        // TODO(crbug.com/dawn/1679): OpenGL backend doesn't support thread safe API yet.
        DAWN_TEST_UNSUPPORTED_IF(IsOpenGL() || IsOpenGLES());
    }

    wgpu::Buffer CreateBuffer(uint32_t size, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = size;
        descriptor.usage = usage;
        return device.CreateBuffer(&descriptor);
    }
};

TEST_P(MultithreadTests, Buffers_MapInParallel) {
    auto threadFunc = [this] {
        constexpr uint32_t kDataSize = 1000;
        std::vector<uint32_t> myData;
        for (uint32_t i = 0; i < kDataSize; ++i) {
            myData.push_back(i);
        }

        constexpr uint32_t kSize = static_cast<uint32_t>(kDataSize * sizeof(uint32_t));
        wgpu::Buffer buffer;
        bool mapCompleted = false;

        // Create buffer and request mapping.
        buffer = CreateBuffer(kSize, wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc);

        buffer.MapAsync(
            wgpu::MapMode::Write, 0, kSize,
            [](WGPUBufferMapAsyncStatus status, void* userdata) {
                EXPECT_EQ(WGPUBufferMapAsyncStatus_Success, status);
                (*static_cast<bool*>(userdata)) = true;
            },
            &mapCompleted);

        // Wait for the mapping to complete
        while (!mapCompleted) {
            device.Tick();
            FlushWire();
        }

        // Buffer is mapped, write into it and unmap .
        memcpy(buffer.GetMappedRange(0, kSize), myData.data(), kSize);
        buffer.Unmap();

        // Check the content of the buffer.
        EXPECT_BUFFER_U32_RANGE_EQ(myData.data(), buffer, 0, kDataSize);
    };

    std::thread thread1(threadFunc);
    std::thread thread2(threadFunc);
    std::thread thread3(threadFunc);

    thread1.join();
    thread2.join();
    thread3.join();
}

class MultithreadDepthStencilCopyTests : public MultithreadTests {
  protected:
    void SetUp() override {
        MultithreadTests::SetUp();

        // TODO(crbug.com/dawn/1291): These tests are failing on GLES (both native and ANGLE)
        // when using Tint/GLSL.
        DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());
    }

    wgpu::Texture CreateTexture(uint32_t width,
                                uint32_t height,
                                wgpu::TextureFormat format,
                                wgpu::TextureUsage usage,
                                uint32_t mipLevelCount = 1) {
        wgpu::TextureDescriptor texDescriptor = {};
        texDescriptor.size = {width, height, 1};
        texDescriptor.format = format;
        texDescriptor.usage = usage;
        texDescriptor.mipLevelCount = mipLevelCount;
        return device.CreateTexture(&texDescriptor);
    }

    wgpu::Texture CreateAndWriteTexture(uint32_t width,
                                        uint32_t height,
                                        wgpu::TextureFormat format,
                                        wgpu::TextureUsage usage,
                                        const void* data,
                                        size_t dataSize) {
        auto texture = CreateTexture(width, height, format, wgpu::TextureUsage::CopyDst | usage);

        wgpu::Extent3D textureSize = {width, height, 1};

        wgpu::ImageCopyTexture imageCopyTexture =
            utils::CreateImageCopyTexture(texture, 0, {0, 0, 0}, wgpu::TextureAspect::All);
        wgpu::TextureDataLayout textureDataLayout =
            utils::CreateTextureDataLayout(0, dataSize / height);

        queue.WriteTexture(&imageCopyTexture, data, dataSize, &textureDataLayout, &textureSize);
        device.Tick();

        return texture;
    }

    uint32_t BufferSizeForTextureCopy(uint32_t width, uint32_t height, wgpu::TextureFormat format) {
        uint32_t bytesPerPixel = utils::GetTexelBlockSizeInBytes(format);
        uint32_t bytesPerRow = Align(width * bytesPerPixel, kTextureBytesPerRowAlignment);
        return bytesPerRow * (height - 1) + width * bytesPerPixel;
    }

    template <typename Step>
    void CopyTextureToTextureInLockStep(LockStep<Step>& lockStep,
                                        const GuardedTexture& srcTexture,
                                        Step stepSrcTextureReady,
                                        const wgpu::Texture& dstTexture,
                                        Step stepDstTextureWritten,
                                        uint32_t dstMipLevel,
                                        const wgpu::Extent3D& dstSize) {
        auto encoder = device.CreateCommandEncoder();

        wgpu::ImageCopyTexture dstView = utils::CreateImageCopyTexture(
            dstTexture, dstMipLevel, {0, 0, 0}, wgpu::TextureAspect::All);

        // Wait until src texture is written.
        lockStep.Wait(stepSrcTextureReady);

        wgpu::ImageCopyTexture srcView =
            utils::CreateImageCopyTexture(srcTexture.Get(), 0, {0, 0, 0}, wgpu::TextureAspect::All);

        encoder.CopyTextureToTexture(&srcView, &dstView, &dstSize);

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        lockStep.Signal(stepDstTextureWritten);
    }

    template <typename Step>
    void CopyBufferToTextureInLockStep(LockStep<Step>& lockStep,
                                       const GuardedBuffer& srcBuffer,
                                       Step stepSrcBufferReady,
                                       uint32_t srcBytesPerRow,
                                       const wgpu::Texture& dstTexture,
                                       Step stepDstTextureWritten,
                                       uint32_t dstMipLevel,
                                       const wgpu::Extent3D& dstSize) {
        auto encoder = device.CreateCommandEncoder();

        wgpu::ImageCopyTexture dstView = utils::CreateImageCopyTexture(
            dstTexture, dstMipLevel, {0, 0, 0}, wgpu::TextureAspect::All);

        // Wait until src buffer is written.
        lockStep.Wait(stepSrcBufferReady);

        wgpu::ImageCopyBuffer srcView =
            utils::CreateImageCopyBuffer(srcBuffer.Get(), 0, srcBytesPerRow, dstSize.height);

        encoder.CopyBufferToTexture(&srcView, &dstView, &dstSize);

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        lockStep.Signal(stepDstTextureWritten);
    }
};

// Use WriteTexture() on one thread, CopyTextureToTexture() on another thread.
TEST_P(MultithreadDepthStencilCopyTests, Depth_WriteAndCopyOnDifferentThreads) {
    enum class Step {
        Begin,
        WriteTexture,
        CopyTexture,
    };

    constexpr uint32_t kWidth = 4;
    constexpr uint32_t kHeight = 4;

    const std::vector<float> kExpectedData32 = {
        0,    0,    0,    0,  //
        0,    0,    0.4f, 0,  //
        1.0f, 1.0f, 0,    0,  //
        1.0f, 1.0f, 0,    0,  //
    };

    std::vector<uint16_t> kExpectedData16(kExpectedData32.size());
    for (size_t i = 0; i < kExpectedData32.size(); ++i) {
        kExpectedData16[i] = kExpectedData32[i] * std::numeric_limits<uint16_t>::max();
    }

    const size_t kExpectedDataSize16 = kExpectedData16.size() * sizeof(kExpectedData16[0]);

    LockStep<Step> lockStep(Step::Begin);

    GuardedTexture depthTexture;
    std::thread writeThread([&] {
        depthTexture = CreateAndWriteTexture(kWidth, kHeight, wgpu::TextureFormat::Depth16Unorm,
                                             wgpu::TextureUsage::CopySrc, kExpectedData16.data(),
                                             kExpectedDataSize16);

        lockStep.Signal(Step::WriteTexture);
        lockStep.Wait(Step::CopyTexture);
    });

    std::thread copyThread([&] {
        auto destTexture =
            CreateTexture(kWidth * 2, kHeight * 2, wgpu::TextureFormat::Depth16Unorm,
                          wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::CopySrc,
                          /*mipLevelCount=*/2);

        // Copy from depthTexture to destTexture.
        const wgpu::Extent3D dstSize = {kWidth, kHeight, 1};
        CopyTextureToTextureInLockStep(lockStep, depthTexture, Step::WriteTexture, destTexture,
                                       Step::CopyTexture, /*dstMipLevel=*/1, dstSize);

        // Verify the copied data
        ExpectAttachmentDepthTestData(destTexture, wgpu::TextureFormat::Depth16Unorm, kWidth,
                                      kHeight, 0, /*mipLevel=*/1, kExpectedData32);
    });

    writeThread.join();
    copyThread.join();
}

// Use WriteBuffer() on one thread, CopyBufferToTexture() on another thread.
TEST_P(MultithreadDepthStencilCopyTests, Depth_WriteBufferAndCopyOnDifferentThreads) {
    enum class Step {
        Begin,
        WriteBuffer,
        CopyTexture,
    };

    constexpr uint32_t kWidth = 16;
    constexpr uint32_t kHeight = 1;

    const std::vector<float> kExpectedData32 = {
        0,    0,    0,    0,  //
        0,    0,    0.4f, 0,  //
        1.0f, 1.0f, 0,    0,  //
        1.0f, 1.0f, 0,    0,  //
    };

    std::vector<uint16_t> kExpectedData16(kExpectedData32.size());
    for (size_t i = 0; i < kExpectedData32.size(); ++i) {
        kExpectedData16[i] = kExpectedData32[i] * std::numeric_limits<uint16_t>::max();
    }

    const uint32_t kExpectedDataSize16 = kExpectedData16.size() * sizeof(kExpectedData16[0]);

    const wgpu::Extent3D kSize = {kWidth, kHeight, 1};
    LockStep<Step> lockStep(Step::Begin);

    GuardedBuffer buffer;
    std::thread writeThread([&] {
        buffer = CreateBuffer(
            BufferSizeForTextureCopy(kWidth, kHeight, wgpu::TextureFormat::Depth16Unorm),
            wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc);

        queue.WriteBuffer(buffer.Get(), 0, kExpectedData16.data(), kExpectedDataSize16);
        device.Tick();

        lockStep.Signal(Step::WriteBuffer);
        lockStep.Wait(Step::CopyTexture);
    });

    std::thread copyThread([&] {
        auto destTexture =
            CreateTexture(kWidth, kHeight, wgpu::TextureFormat::Depth16Unorm,
                          wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::CopySrc);

        CopyBufferToTextureInLockStep(lockStep, buffer, Step::WriteBuffer,
                                      kTextureBytesPerRowAlignment, destTexture, Step::CopyTexture,
                                      /*dstMipLevel=*/0, kSize);

        // Verify the copied data
        ExpectAttachmentDepthTestData(destTexture, wgpu::TextureFormat::Depth16Unorm, kWidth,
                                      kHeight, 0, /*mipLevel=*/0, kExpectedData32);
    });

    writeThread.join();
    copyThread.join();
}

// Use WriteTexture() on one thread, CopyTextureToTexture() on another thread.
TEST_P(MultithreadDepthStencilCopyTests, Stencil_WriteAndCopyOnDifferentThreads) {
    // TODO(crbug.com/dawn/1497): glReadPixels: GL error: HIGH: Invalid format and type
    // combination.
    DAWN_SUPPRESS_TEST_IF(IsANGLE());

    // TODO(crbug.com/dawn/667): Work around the fact that some platforms are unable to read
    // stencil.
    DAWN_TEST_UNSUPPORTED_IF(HasToggleEnabled("disable_depth_stencil_read"));

    enum class Step {
        Begin,
        WriteTexture,
        CopyTexture,
    };

    constexpr uint32_t kWidth = 1;
    constexpr uint32_t kHeight = 1;

    constexpr uint8_t kExpectedData = 177;
    constexpr size_t kExpectedDataSize = sizeof(kExpectedData);

    LockStep<Step> lockStep(Step::Begin);

    GuardedTexture stencilTexture;
    std::thread writeThread([&] {
        stencilTexture =
            CreateAndWriteTexture(kWidth, kHeight, wgpu::TextureFormat::Stencil8,
                                  wgpu::TextureUsage::CopySrc, &kExpectedData, kExpectedDataSize);

        lockStep.Signal(Step::WriteTexture);
        lockStep.Wait(Step::CopyTexture);
    });

    std::thread copyThread([&] {
        auto destTexture =
            CreateTexture(kWidth * 2, kHeight * 2, wgpu::TextureFormat::Stencil8,
                          wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::CopySrc,
                          /*mipLevelCount=*/2);

        // Copy from stencilTexture to destTexture.
        const wgpu::Extent3D dstSize = {kWidth, kHeight, 1};
        CopyTextureToTextureInLockStep(lockStep, stencilTexture, Step::WriteTexture, destTexture,
                                       Step::CopyTexture, /*dstMipLevel=*/1, dstSize);

        // Verify the copied data
        ExpectAttachmentStencilTestData(destTexture, wgpu::TextureFormat::Stencil8, kWidth, kHeight,
                                        0, /*mipLevel=*/1, kExpectedData);
    });

    writeThread.join();
    copyThread.join();
}

// Use WriteBuffer() on one thread, CopyBufferToTexture() on another thread.
TEST_P(MultithreadDepthStencilCopyTests, Stencil_WriteBufferAndCopyOnDifferentThreads) {
    enum class Step {
        Begin,
        WriteBuffer,
        CopyTexture,
    };

    constexpr uint32_t kWidth = 1;
    constexpr uint32_t kHeight = 1;

    constexpr uint8_t kExpectedData = 177;

    const wgpu::Extent3D kSize = {kWidth, kHeight, 1};
    LockStep<Step> lockStep(Step::Begin);

    GuardedBuffer buffer;
    std::thread writeThread([&] {
        const auto kBufferSize = kTextureBytesPerRowAlignment;
        buffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc);

        std::vector<uint8_t> bufferData(kBufferSize);
        bufferData[0] = kExpectedData;

        queue.WriteBuffer(buffer.Get(), 0, bufferData.data(), kBufferSize);
        device.Tick();

        lockStep.Signal(Step::WriteBuffer);
        lockStep.Wait(Step::CopyTexture);
    });

    std::thread copyThread([&] {
        auto destTexture =
            CreateTexture(kWidth, kHeight, wgpu::TextureFormat::Stencil8,
                          wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::CopySrc);

        CopyBufferToTextureInLockStep(lockStep, buffer, Step::WriteBuffer,
                                      kTextureBytesPerRowAlignment, destTexture, Step::CopyTexture,
                                      /*dstMipLevel=*/0, kSize);

        // Verify the copied data
        ExpectAttachmentStencilTestData(destTexture, wgpu::TextureFormat::Stencil8, kWidth, kHeight,
                                        0, /*mipLevel=*/0, kExpectedData);
    });

    writeThread.join();
    copyThread.join();
}

DAWN_INSTANTIATE_TEST(MultithreadTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());

DAWN_INSTANTIATE_TEST(
    MultithreadDepthStencilCopyTests,
    D3D12Backend(),
    MetalBackend(),
    MetalBackend({"use_blit_for_buffer_to_depth_texture_copy",
                  "use_blit_for_depth_texture_to_texture_copy_to_nonzero_subresource"}),
    MetalBackend({"use_blit_for_buffer_to_stencil_texture_copy"}),
    OpenGLBackend(),
    OpenGLESBackend(),
    VulkanBackend());
