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
#include <random>
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

}  // namespace

class MultithreadTests : public DawnTest {
  protected:
    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        std::vector<wgpu::FeatureName> features;
        // TODO(crbug.com/dawn/1678): DawnWire doesn't support thread safe API yet.
        if (!UsesWire()) {
            features.push_back(wgpu::FeatureName::ImplicitDeviceSynchronization);
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
                                uint32_t mipLevelCount = 1,
                                uint32_t sampleCount = 1) {
        wgpu::TextureDescriptor texDescriptor = {};
        texDescriptor.size = {width, height, 1};
        texDescriptor.format = format;
        texDescriptor.usage = usage;
        texDescriptor.mipLevelCount = mipLevelCount;
        texDescriptor.sampleCount = sampleCount;
        return device.CreateTexture(&texDescriptor);
    }
};

// Test that dropping a device's last ref on another thread won't crash Instance::ProcessEvents.
TEST_P(MultithreadTests, Device_DroppedOnAnotherThread) {
    enum class Step {
        Begin,
        MainRefDropped,
    };

    std::vector<wgpu::Device> devices(5);
    std::vector<std::unique_ptr<LockStep<Step>>> lockSteps(devices.size());
    std::vector<std::unique_ptr<std::thread>> threads(devices.size());

    std::mt19937 sleepTimeGenerator;
    std::uniform_int_distribution<uint32_t> sleepTimeDist(10, 100);

    // Create devices.
    for (size_t i = 0; i < devices.size(); ++i) {
        devices[i] = CreateDevice();
        lockSteps[i] = std::make_unique<LockStep<Step>>(Step::Begin);
    }

    // Create threads
    for (size_t i = 0; i < devices.size(); ++i) {
        auto& additionalDevice = devices[i];
        auto& lockStep = *lockSteps[i];

        uint32_t threadSleepTime = sleepTimeDist(sleepTimeGenerator);

        threads[i] = std::make_unique<std::thread>([&lockStep, additionalDevice, threadSleepTime] {
            additionalDevice.PushErrorScope(wgpu::ErrorFilter::Validation);

            std::atomic_bool done(false);
            additionalDevice.PopErrorScope(
                [](WGPUErrorType type, const char*, void* userdataPtr) {
                    auto userdata = static_cast<std::atomic_bool*>(userdataPtr);
                    *userdata = true;
                },
                &done);
            additionalDevice.Tick();
            lockStep.Wait(Step::MainRefDropped);

            EXPECT_TRUE(done.load());

            std::this_thread::sleep_for(std::chrono::milliseconds(threadSleepTime));
            // additionalDevice's 2nd ref dropped
        });

        // additionalDevice's 1st ref drop
        additionalDevice = nullptr;
        lockStep.Signal(Step::MainRefDropped);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeDist(sleepTimeGenerator) + 5));
    WaitABit();

    for (auto& thread : threads) {
        thread->join();
    }
}

// Test that dropping a device's last ref inside a callback on another thread won't crash
// Instance::ProcessEvents.
TEST_P(MultithreadTests, Device_DroppedInCallback_OnAnotherThread) {
    std::vector<wgpu::Device> devices(5);
    std::vector<std::unique_ptr<std::thread>> threads(devices.size());

    // Create devices.
    for (size_t i = 0; i < devices.size(); ++i) {
        devices[i] = CreateDevice();
    }

    // Create threads
    for (size_t i = 0; i < devices.size(); ++i) {
        auto& additionalDevice = devices[i];

        // Move additional device's main ref to a thread.
        threads[i] =
            std::make_unique<std::thread>([additionalDevicePtr = additionalDevice.Release(), this] {
                auto additionalDevice = wgpu::Device::Acquire(additionalDevicePtr);
                struct UserData {
                    wgpu::Device device2ndRef;
                    std::atomic_bool isCompleted{false};
                } userData;

                userData.device2ndRef = additionalDevice;

                // Drop the last ref inside a callback.
                additionalDevice.PushErrorScope(wgpu::ErrorFilter::Validation);
                additionalDevice.PopErrorScope(
                    [](WGPUErrorType type, const char*, void* userdataPtr) {
                        auto userdata = static_cast<UserData*>(userdataPtr);
                        userdata->device2ndRef = nullptr;
                        userdata->isCompleted = true;
                    },
                    &userData);
                // main ref dropped.
                additionalDevice = nullptr;

                do {
                    WaitABit();
                } while (!userData.isCompleted.load());

                EXPECT_EQ(userData.device2ndRef, nullptr);
            });
    }

    WaitABit();

    for (auto& thread : threads) {
        thread->join();
    }
}

// Test that multiple buffers being created and mapped on multiple threads won't interfere with each
// other.
TEST_P(MultithreadTests, Buffers_MapInParallel) {
    constexpr uint32_t kDataSize = 1000;
    std::vector<uint32_t> myData;
    for (uint32_t i = 0; i < kDataSize; ++i) {
        myData.push_back(i);
    }

    constexpr uint32_t kSize = static_cast<uint32_t>(kDataSize * sizeof(uint32_t));

    auto ThreadFunc = [=, &myData = std::as_const(myData)] {
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

    std::thread thread1(ThreadFunc);
    std::thread thread2(ThreadFunc);
    std::thread thread3(ThreadFunc);

    thread1.join();
    thread2.join();
    thread3.join();
}

// Test that encoding render passes in parallel should work
TEST_P(MultithreadTests, RenderPassEncodersInParallel) {
    constexpr uint32_t kRTSize = 16;

    wgpu::Texture msaaRenderTarget =
        CreateTexture(kRTSize, kRTSize, wgpu::TextureFormat::RGBA8Unorm,
                      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
                      /*mipLevelCount=*/1, /*sampleCount=*/4);
    wgpu::TextureView msaaRenderTargetView = msaaRenderTarget.CreateView();

    // Resolve to mip level = 1 to force render pass workarounds (there shouldn't be any deadlock
    // happening).
    wgpu::Texture resolveTarget =
        CreateTexture(kRTSize * 2, kRTSize * 2, wgpu::TextureFormat::RGBA8Unorm,
                      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
                      /*mipLevelCount=*/2, /*sampleCount=*/1);
    wgpu::TextureViewDescriptor resolveTargetViewDesc;
    resolveTargetViewDesc.baseMipLevel = 1;
    resolveTargetViewDesc.mipLevelCount = 1;
    wgpu::TextureView resolveTargetView = resolveTarget.CreateView(&resolveTargetViewDesc);

    std::vector<std::unique_ptr<std::thread>> threads(10);
    std::vector<wgpu::CommandBuffer> commandBuffers(threads.size());

    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i] = std::make_unique<std::thread>([=, &commandBuffers] {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

            // Clear the renderTarget to red.
            utils::ComboRenderPassDescriptor renderPass({msaaRenderTargetView});
            renderPass.cColorAttachments[0].resolveTarget = resolveTargetView;
            renderPass.cColorAttachments[0].clearValue = {1.0f, 0.0f, 0.0f, 1.0f};

            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
            pass.End();

            commandBuffers[i] = encoder.Finish();
        });
    }

    for (auto& thread : threads) {
        thread->join();
    }

    // Verify command buffers
    for (auto& commandBuffer : commandBuffers) {
        queue.Submit(1, &commandBuffer);

        EXPECT_TEXTURE_EQ(utils::RGBA8::kRed, resolveTarget, {0, 0}, 1);
        EXPECT_TEXTURE_EQ(utils::RGBA8::kRed, resolveTarget, {kRTSize - 1, kRTSize - 1}, 1);
    }
}

// Test that encoding compute passes in parallel should work
TEST_P(MultithreadTests, ComputePassEncodersInParallel) {
    constexpr uint32_t kExpected = 0xFFFFFFFFu;
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
            @group(0) @binding(0) var<storage, read_write> output : u32;

            @compute @workgroup_size(1, 1, 1)
            fn main(@builtin(global_invocation_id) GlobalInvocationID : vec3u) {
                output = 0xFFFFFFFFu;
            })");
    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.compute.module = module;
    csDesc.compute.entryPoint = "main";
    auto pipeline = device.CreateComputePipeline(&csDesc);

    wgpu::Buffer dstBuffer =
        CreateBuffer(sizeof(uint32_t), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc |
                                           wgpu::BufferUsage::CopyDst);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                     {
                                                         {0, dstBuffer, 0, sizeof(uint32_t)},
                                                     });

    std::vector<std::unique_ptr<std::thread>> threads(10);
    std::vector<wgpu::CommandBuffer> commandBuffers(threads.size());

    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i] = std::make_unique<std::thread>([=, &commandBuffers] {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, bindGroup);
            pass.DispatchWorkgroups(1, 1, 1);
            pass.End();

            commandBuffers[i] = encoder.Finish();
        });
    }

    for (auto& thread : threads) {
        thread->join();
    }

    // Verify command buffers
    for (auto& commandBuffer : commandBuffers) {
        constexpr uint32_t kSentinelData = 0;
        queue.WriteBuffer(dstBuffer, 0, &kSentinelData, sizeof(kSentinelData));
        queue.Submit(1, &commandBuffer);

        EXPECT_BUFFER_U32_EQ(kExpected, dstBuffer, 0);
    }
}

// Test CreateComputePipelineAsync on multiple threads.
TEST_P(MultithreadTests, CreateComputePipelineAsyncInParallel) {
    const auto CreatePipelineFunc = [this]() -> wgpu::ComputePipeline {
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
            wgpu::ComputePipeline computePipeline;
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

    const auto VerifyPipelineFunc = [this](const wgpu::ComputePipeline& pipeline) {
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

    std::vector<wgpu::ComputePipeline> pipelines(10);
    std::vector<std::unique_ptr<std::thread>> threads(pipelines.size());
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i] = std::make_unique<std::thread>(
            [&pipelines, i, CreatePipelineFunc] { pipelines[i] = CreatePipelineFunc(); });
    }

    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i]->join();
        VerifyPipelineFunc(pipelines[i].Get());
    }
}

// Test CreateRenderPipelineAsync on multiple threads.
TEST_P(MultithreadTests, CreateRenderPipelineAsyncInParallel) {
    constexpr wgpu::TextureFormat kRenderAttachmentFormat = wgpu::TextureFormat::RGBA8Unorm;

    const auto CreatePipelineFunc = [=]() -> wgpu::RenderPipeline {
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
            wgpu::RenderPipeline renderPipeline;
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

    const auto VerifyPipelineFunc = [=](const wgpu::RenderPipeline& pipeline) {
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

    std::vector<wgpu::RenderPipeline> pipelines(10);
    std::vector<std::unique_ptr<std::thread>> threads(pipelines.size());
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i] = std::make_unique<std::thread>(
            [&pipelines, i, CreatePipelineFunc] { pipelines[i] = CreatePipelineFunc(); });
    }

    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i]->join();
        VerifyPipelineFunc(pipelines[i].Get());
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

        return texture;
    }

    uint32_t BufferSizeForTextureCopy(uint32_t width, uint32_t height, wgpu::TextureFormat format) {
        uint32_t bytesPerRow = utils::GetMinimumBytesPerRow(format, width);
        return utils::RequiredBytesInCopy(bytesPerRow, height, {width, height, 1}, format);
    }

    template <typename Step>
    void CopyTextureToTextureInLockStep(
        LockStep<Step>& lockStep,
        const wgpu::Texture& srcTexture,
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
                                       const wgpu::Buffer& srcBuffer,
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
// This is for depth texture.
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

    wgpu::Texture depthTexture;
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
// This is for depth texture.
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

    wgpu::Buffer buffer;
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
// This is for stencil texture.
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

    wgpu::Texture stencilTexture;
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
// This is for stencil texture.
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

    wgpu::Buffer buffer;
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
// The texture under test is color formatted.
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

    wgpu::Texture srcTexture;
    std::thread writeThread([&] {
        srcTexture =
            CreateAndWriteTexture(kWidth, kHeight, wgpu::TextureFormat::RGBA8Unorm,
                                  wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::TextureBinding,
                                  kExpectedData.data(), kExpectedDataSize);

        lockStep.Signal(Step::WriteTexture);
        lockStep.Wait(Step::CopyTexture);

        // Verify the initial data
        EXPECT_TEXTURE_EQ(kExpectedData.data(), srcTexture, {0, 0}, {kWidth, kHeight});
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
        EXPECT_TEXTURE_EQ(kExpectedFlippedData.data(), destTexture, {0, 0}, {kWidth, kHeight});
    });

    writeThread.join();
    copyThread.join();
}

// Test that error from CopyTextureForBrowser() won't cause deadlock.
TEST_P(MultithreadTextureCopyTests, CopyForBrowserErrorNoDeadLock) {
    // TODO(crbug.com/dawn/1232): Program link error on OpenGLES backend
    DAWN_SUPPRESS_TEST_IF(IsOpenGLES());
    DAWN_SUPPRESS_TEST_IF(IsOpenGL() && IsLinux());

    DAWN_TEST_UNSUPPORTED_IF(HasToggleEnabled("skip_validation"));

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

    wgpu::Texture srcTexture;
    std::thread writeThread([&] {
        srcTexture =
            CreateAndWriteTexture(kWidth, kHeight, wgpu::TextureFormat::RGBA8Unorm,
                                  wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::TextureBinding,
                                  kExpectedData.data(), kExpectedDataSize);

        lockStep.Signal(Step::WriteTexture);
        lockStep.Wait(Step::CopyTexture);

        // Verify the initial data
        EXPECT_TEXTURE_EQ(kExpectedData.data(), srcTexture, {0, 0}, {kWidth, kHeight});
    });

    std::thread copyThread([&] {
        wgpu::Texture invalidSrcTexture;
        invalidSrcTexture = CreateTexture(kWidth, kHeight, wgpu::TextureFormat::RGBA8Unorm,
                                          wgpu::TextureUsage::CopySrc);
        auto destTexture =
            CreateTexture(kWidth, kHeight, wgpu::TextureFormat::RGBA8Unorm,
                          wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::CopySrc);

        // Copy from srcTexture to destTexture.
        const wgpu::Extent3D dstSize = {kWidth, kHeight, 1};
        wgpu::CopyTextureForBrowserOptions options = {};

        device.PushErrorScope(wgpu::ErrorFilter::Validation);

        // The first copy should be an error because of missing TextureBinding from src texture.
        CopyTextureToTextureInLockStep(lockStep, invalidSrcTexture, Step::WriteTexture, destTexture,
                                       Step::CopyTextureError,
                                       /*dstMipLevel=*/0, dstSize, &options);

        std::atomic<bool> errorThrown(false);
        device.PopErrorScope(
            [](WGPUErrorType type, char const* message, void* userdata) {
                EXPECT_EQ(type, WGPUErrorType_Validation);
                auto error = static_cast<std::atomic<bool>*>(userdata);
                *error = true;
            },
            &errorThrown);
        device.Tick();
        EXPECT_TRUE(errorThrown.load());

        // Second copy is valid.
        CopyTextureToTextureInLockStep(lockStep, srcTexture, Step::CopyTextureError, destTexture,
                                       Step::CopyTexture,
                                       /*dstMipLevel=*/0, dstSize, &options);

        // Verify the copied data
        EXPECT_TEXTURE_EQ(kExpectedData.data(), destTexture, {0, 0}, {kWidth, kHeight});
    });

    writeThread.join();
    copyThread.join();
}

DAWN_INSTANTIATE_TEST(MultithreadTests,
                      D3D12Backend(),
                      D3D12Backend({"always_resolve_into_zero_level_and_layer"}),
                      MetalBackend(),
                      MetalBackend({"always_resolve_into_zero_level_and_layer"}),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend(),
                      VulkanBackend({"always_resolve_into_zero_level_and_layer"}));

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
