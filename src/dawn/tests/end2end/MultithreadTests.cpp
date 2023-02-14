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

#include <atomic>
#include <condition_variable>
#include <limits>
#include <memory>
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
    GuardedObject() = default;
    explicit GuardedObject(const T& obj) : mObj(obj) {}

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
using GuardedComputePipeline = GuardedObject<wgpu::ComputePipeline>;
using GuardedRenderPipeline = GuardedObject<wgpu::RenderPipeline>;
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
        std::atomic<bool> mapCompleted(false);

        // Create buffer and request mapping.
        buffer = CreateBuffer(kSize, wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc);

        buffer.MapAsync(
            wgpu::MapMode::Write, 0, kSize,
            [](WGPUBufferMapAsyncStatus status, void* userdata) {
                EXPECT_EQ(WGPUBufferMapAsyncStatus_Success, status);
                (*static_cast<std::atomic<bool>*>(userdata)) = true;
            },
            &mapCompleted);

        // Wait for the mapping to complete
        while (!mapCompleted.load()) {
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

TEST_P(MultithreadTests, CreateComputePipelineAsyncInParallel) {
    const auto createPipelineFunc = [this]() -> wgpu::ComputePipeline {
        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.compute.module = utils::CreateShaderModule(device, R"(
        struct SSBO {
            value : u32
        }
        @group(0) @binding(0) var<storage, read_write> ssbo : SSBO;

        @compute @workgroup_size(1) fn main() {
            ssbo.value = 1u;
        })");
        csDesc.compute.entryPoint = "main";

        struct Task {
            GuardedComputePipeline computePipeline;
            std::atomic<bool> isCompleted{false};
        } task;
        device.CreateComputePipelineAsync(
            &csDesc,
            [](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline returnPipeline,
               const char* message, void* userdata) {
                EXPECT_EQ(WGPUCreatePipelineAsyncStatus::WGPUCreatePipelineAsyncStatus_Success,
                          status);

                auto task = static_cast<Task*>(userdata);
                task->computePipeline = wgpu::ComputePipeline::Acquire(returnPipeline);
                task->isCompleted = true;
            },
            &task);

        while (!task.isCompleted.load()) {
            WaitABit();
        }

        return task.computePipeline.Get();
    };  // createPipelineFunc

    const auto verifyPipelineFunc = [this](const wgpu::ComputePipeline& pipeline) {
        wgpu::Buffer ssbo =
            CreateBuffer(sizeof(uint32_t), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);

        wgpu::CommandBuffer commands;
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();

            ASSERT_NE(nullptr, pipeline.Get());
            wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                             {
                                                                 {0, ssbo, 0, sizeof(uint32_t)},
                                                             });
            pass.SetBindGroup(0, bindGroup);
            pass.SetPipeline(pipeline);

            pass.DispatchWorkgroups(1);
            pass.End();

            commands = encoder.Finish();
        }

        queue.Submit(1, &commands);

        constexpr uint32_t kExpected = 1u;
        EXPECT_BUFFER_U32_EQ(kExpected, ssbo, 0);
    };  // verifyPipelineFunc

    std::vector<GuardedComputePipeline> pipelines(10);
    std::vector<std::unique_ptr<std::thread>> threads(pipelines.size());
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i] = std::make_unique<std::thread>(
            [&pipelines, i, createPipelineFunc] { pipelines[i] = createPipelineFunc(); });
    }

    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i]->join();
        verifyPipelineFunc(pipelines[i].Get());
    }
}

TEST_P(MultithreadTests, CreateRenderPipelineAsyncInParallel) {
    constexpr wgpu::TextureFormat kRenderAttachmentFormat = wgpu::TextureFormat::RGBA8Unorm;

    const auto createPipelineFunc = [=]() -> wgpu::RenderPipeline {
        utils::ComboRenderPipelineDescriptor renderPipelineDescriptor;
        wgpu::ShaderModule vsModule = utils::CreateShaderModule(device, R"(
        @vertex fn main() -> @builtin(position) vec4f {
            return vec4f(0.0, 0.0, 0.0, 1.0);
        })");
        wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, R"(
        @fragment fn main() -> @location(0) vec4f {
            return vec4f(0.0, 1.0, 0.0, 1.0);
        })");
        renderPipelineDescriptor.vertex.module = vsModule;
        renderPipelineDescriptor.cFragment.module = fsModule;
        renderPipelineDescriptor.cTargets[0].format = kRenderAttachmentFormat;
        renderPipelineDescriptor.primitive.topology = wgpu::PrimitiveTopology::PointList;

        struct Task {
            GuardedRenderPipeline renderPipeline;
            std::atomic<bool> isCompleted{false};
        } task;
        device.CreateRenderPipelineAsync(
            &renderPipelineDescriptor,
            [](WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline returnPipeline,
               const char* message, void* userdata) {
                EXPECT_EQ(WGPUCreatePipelineAsyncStatus::WGPUCreatePipelineAsyncStatus_Success,
                          status);

                auto* task = static_cast<Task*>(userdata);
                task->renderPipeline = wgpu::RenderPipeline::Acquire(returnPipeline);
                task->isCompleted = true;
            },
            &task);

        while (!task.isCompleted) {
            WaitABit();
        }

        return task.renderPipeline.Get();
    };  // createPipelineFunc

    const auto verifyPipelineFunc = [=](const wgpu::RenderPipeline& pipeline) {
        wgpu::Texture outputTexture =
            CreateTexture(1, 1, kRenderAttachmentFormat,
                          wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc);

        utils::ComboRenderPassDescriptor renderPassDescriptor({outputTexture.CreateView()});
        renderPassDescriptor.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
        renderPassDescriptor.cColorAttachments[0].clearValue = {1.f, 0.f, 0.f, 1.f};

        wgpu::CommandBuffer commands;
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder renderPassEncoder =
                encoder.BeginRenderPass(&renderPassDescriptor);

            ASSERT_NE(nullptr, pipeline.Get());

            renderPassEncoder.SetPipeline(pipeline);
            renderPassEncoder.Draw(1);
            renderPassEncoder.End();
            commands = encoder.Finish();
        }

        queue.Submit(1, &commands);

        EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8(0, 255, 0, 255), outputTexture, 0, 0);
    };  // verifyPipelineFunc

    std::vector<GuardedRenderPipeline> pipelines(10);
    std::vector<std::unique_ptr<std::thread>> threads(pipelines.size());
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i] = std::make_unique<std::thread>(
            [&pipelines, i, createPipelineFunc] { pipelines[i] = createPipelineFunc(); });
    }

    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i]->join();
        verifyPipelineFunc(pipelines[i].Get());
    }
}

class MultithreadTextureCopyTests : public MultithreadTests {
  protected:
    void SetUp() override {
        MultithreadTests::SetUp();

        // TODO(crbug.com/dawn/1291): These tests are failing on GLES (both native and ANGLE)
        // when using Tint/GLSL.
        DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());
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
    void CopyTextureToTextureInLockStep(
        LockStep<Step>& lockStep,
        const GuardedTexture& srcTexture,
        Step stepSrcTextureReady,
        const wgpu::Texture& dstTexture,
        Step stepDstTextureWritten,
        uint32_t dstMipLevel,
        const wgpu::Extent3D& dstSize,
        const wgpu::CopyTextureForBrowserOptions* copyForBrowerOptions = nullptr) {
        wgpu::CommandEncoder encoder;

        if (copyForBrowerOptions == nullptr) {
            encoder = device.CreateCommandEncoder();
        }

        wgpu::ImageCopyTexture dstView = utils::CreateImageCopyTexture(
            dstTexture, dstMipLevel, {0, 0, 0}, wgpu::TextureAspect::All);

        // Wait until src texture is written.
        lockStep.Wait(stepSrcTextureReady);

        wgpu::ImageCopyTexture srcView =
            utils::CreateImageCopyTexture(srcTexture.Get(), 0, {0, 0, 0}, wgpu::TextureAspect::All);

        if (copyForBrowerOptions == nullptr) {
            encoder.CopyTextureToTexture(&srcView, &dstView, &dstSize);

            wgpu::CommandBuffer commands = encoder.Finish();
            queue.Submit(1, &commands);
        } else {
            queue.CopyTextureForBrowser(&srcView, &dstView, &dstSize, copyForBrowerOptions);
        }

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

    template <typename T>
    void ExpectColorTestData(const wgpu::Texture& texture,
                             wgpu::TextureFormat format,
                             uint32_t width,
                             uint32_t height,
                             const std::vector<T>& testData) {
        for (uint32_t idx = 0, r = 0; r < height; ++r) {
            for (uint32_t c = 0; c < width; ++c, ++idx) {
                switch (format) {
                    case wgpu::TextureFormat::RGBA8Unorm:
                        EXPECT_PIXEL_RGBA8_EQ(testData[idx], texture, c, r)
                            << " Failed at " << r << " " << c;
                        break;
                    case wgpu::TextureFormat::R32Float:
                        EXPECT_PIXEL_FLOAT_EQ(testData[idx], texture, c, r)
                            << " Failed at " << r << " " << c;
                        break;
                    default:
                        // unsupported
                        abort();
                }
            }
        }
    }
};

// Use WriteTexture() on one thread, CopyTextureToTexture() on another thread.
TEST_P(MultithreadTextureCopyTests, Depth_WriteAndCopyOnDifferentThreads) {
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
TEST_P(MultithreadTextureCopyTests, Depth_WriteBufferAndCopyOnDifferentThreads) {
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
TEST_P(MultithreadTextureCopyTests, Stencil_WriteAndCopyOnDifferentThreads) {
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
TEST_P(MultithreadTextureCopyTests, Stencil_WriteBufferAndCopyOnDifferentThreads) {
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

// Use WriteTexture() on one thread, CopyTextureForBrowser() on another thread.
TEST_P(MultithreadTextureCopyTests, Color_WriteAndCopyForBrowserOnDifferentThreads) {
    // TODO(crbug.com/dawn/1232): Program link error on OpenGLES backend
    DAWN_SUPPRESS_TEST_IF(IsOpenGLES());
    DAWN_SUPPRESS_TEST_IF(IsOpenGL() && IsLinux());

    enum class Step {
        Begin,
        WriteTexture,
        CopyTexture,
    };

    constexpr uint32_t kWidth = 4;
    constexpr uint32_t kHeight = 4;

    const std::vector<utils::RGBA8> kExpectedData = {
        utils::RGBA8::kBlack, utils::RGBA8::kBlack, utils::RGBA8::kBlack, utils::RGBA8::kBlack,  //
        utils::RGBA8::kBlack, utils::RGBA8::kBlack, utils::RGBA8::kGreen, utils::RGBA8::kBlack,  //
        utils::RGBA8::kRed,   utils::RGBA8::kRed,   utils::RGBA8::kBlack, utils::RGBA8::kBlack,  //
        utils::RGBA8::kRed,   utils::RGBA8::kBlue,  utils::RGBA8::kBlack, utils::RGBA8::kBlack,  //
    };

    const std::vector<utils::RGBA8> kExpectedFlippedData = {
        utils::RGBA8::kRed,   utils::RGBA8::kBlue,  utils::RGBA8::kBlack, utils::RGBA8::kBlack,  //
        utils::RGBA8::kRed,   utils::RGBA8::kRed,   utils::RGBA8::kBlack, utils::RGBA8::kBlack,  //
        utils::RGBA8::kBlack, utils::RGBA8::kBlack, utils::RGBA8::kGreen, utils::RGBA8::kBlack,  //
        utils::RGBA8::kBlack, utils::RGBA8::kBlack, utils::RGBA8::kBlack, utils::RGBA8::kBlack,  //
    };

    const size_t kExpectedDataSize = kExpectedData.size() * sizeof(kExpectedData[0]);

    LockStep<Step> lockStep(Step::Begin);

    GuardedTexture srcTexture;
    std::thread writeThread([&] {
        srcTexture =
            CreateAndWriteTexture(kWidth, kHeight, wgpu::TextureFormat::RGBA8Unorm,
                                  wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::TextureBinding,
                                  kExpectedData.data(), kExpectedDataSize);

        lockStep.Signal(Step::WriteTexture);
        lockStep.Wait(Step::CopyTexture);

        // Verify the initial data
        ExpectColorTestData(srcTexture.Get(), wgpu::TextureFormat::RGBA8Unorm, kWidth, kHeight,
                            kExpectedData);
    });

    std::thread copyThread([&] {
        auto destTexture =
            CreateTexture(kWidth, kHeight, wgpu::TextureFormat::RGBA8Unorm,
                          wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::CopySrc);

        // Copy from srcTexture to destTexture.
        const wgpu::Extent3D dstSize = {kWidth, kHeight, 1};
        wgpu::CopyTextureForBrowserOptions options;
        options.flipY = true;
        CopyTextureToTextureInLockStep(lockStep, srcTexture, Step::WriteTexture, destTexture,
                                       Step::CopyTexture, /*dstMipLevel=*/0, dstSize, &options);

        // Verify the copied data
        ExpectColorTestData(destTexture.Get(), wgpu::TextureFormat::RGBA8Unorm, kWidth, kHeight,
                            kExpectedFlippedData);
    });

    writeThread.join();
    copyThread.join();
}

// Test that error from CopyTextureForBrowser() won't cause deadlock.
TEST_P(MultithreadTextureCopyTests, CopyForBrowserErrorNoDeadLock) {
    // TODO(crbug.com/dawn/1232): Program link error on OpenGLES backend
    DAWN_SUPPRESS_TEST_IF(IsOpenGLES());
    DAWN_SUPPRESS_TEST_IF(IsOpenGL() && IsLinux());

    enum class Step {
        Begin,
        WriteTexture,
        CopyTextureError,
        CopyTexture,
    };

    constexpr uint32_t kWidth = 4;
    constexpr uint32_t kHeight = 4;

    const std::vector<utils::RGBA8> kExpectedData = {
        utils::RGBA8::kBlack, utils::RGBA8::kBlack, utils::RGBA8::kBlack, utils::RGBA8::kBlack,  //
        utils::RGBA8::kBlack, utils::RGBA8::kBlack, utils::RGBA8::kGreen, utils::RGBA8::kBlack,  //
        utils::RGBA8::kRed,   utils::RGBA8::kRed,   utils::RGBA8::kBlack, utils::RGBA8::kBlack,  //
        utils::RGBA8::kRed,   utils::RGBA8::kBlue,  utils::RGBA8::kBlack, utils::RGBA8::kBlack,  //
    };

    const size_t kExpectedDataSize = kExpectedData.size() * sizeof(kExpectedData[0]);

    LockStep<Step> lockStep(Step::Begin);

    GuardedTexture srcTexture;
    std::thread writeThread([&] {
        srcTexture =
            CreateAndWriteTexture(kWidth, kHeight, wgpu::TextureFormat::RGBA8Unorm,
                                  wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::TextureBinding,
                                  kExpectedData.data(), kExpectedDataSize);

        lockStep.Signal(Step::WriteTexture);
        lockStep.Wait(Step::CopyTexture);

        // Verify the initial data
        ExpectColorTestData(srcTexture.Get(), wgpu::TextureFormat::RGBA8Unorm, kWidth, kHeight,
                            kExpectedData);
    });

    std::thread copyThread([&] {
        GuardedTexture invalidSrcTexture;
        invalidSrcTexture = CreateTexture(kWidth, kHeight, wgpu::TextureFormat::RGBA8Unorm,
                                          wgpu::TextureUsage::CopySrc);
        auto destTexture =
            CreateTexture(kWidth, kHeight, wgpu::TextureFormat::RGBA8Unorm,
                          wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::CopySrc);

        // Copy from srcTexture to destTexture.
        const wgpu::Extent3D dstSize = {kWidth, kHeight, 1};
        wgpu::CopyTextureForBrowserOptions options = {};

        std::atomic<bool> errorThrown(false);

        device.SetUncapturedErrorCallback(
            [](WGPUErrorType type, char const* message, void* userdata) {
                EXPECT_EQ(type, WGPUErrorType_Validation);
                auto error = static_cast<std::atomic<bool>*>(userdata);
                *error = true;
            },
            &errorThrown);

        // The fist copy should be an error because of missing TextureBinding from src texture.
        CopyTextureToTextureInLockStep(lockStep, invalidSrcTexture, Step::WriteTexture, destTexture,
                                       Step::CopyTextureError,
                                       /*dstMipLevel=*/0, dstSize, &options);
        device.Tick();
        EXPECT_TRUE(errorThrown.load());

        // Second copy is valid.
        CopyTextureToTextureInLockStep(lockStep, srcTexture, Step::CopyTextureError, destTexture,
                                       Step::CopyTexture,
                                       /*dstMipLevel=*/0, dstSize, &options);

        // Verify the copied data
        ExpectColorTestData(destTexture.Get(), wgpu::TextureFormat::RGBA8Unorm, kWidth, kHeight,
                            kExpectedData);
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
    MultithreadTextureCopyTests,
    D3D12Backend(),
    MetalBackend(),
    MetalBackend({"use_blit_for_buffer_to_depth_texture_copy",
                  "use_blit_for_depth_texture_to_texture_copy_to_nonzero_subresource"}),
    MetalBackend({"use_blit_for_buffer_to_stencil_texture_copy"}),
    OpenGLBackend(),
    OpenGLESBackend(),
    VulkanBackend());
