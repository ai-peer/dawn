// Copyright 2017 The Dawn Authors
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

#include <poll.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "dawn/samples/SampleUtils.h"

#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/SystemUtils.h"
#include "dawn/utils/Timer.h"
#include "dawn/utils/WGPUHelpers.h"

wgpu::Device device;
wgpu::Queue queue;
wgpu::SwapChain swapchain;
wgpu::RenderPipeline pipeline;
wgpu::BindGroup bindGroup;
wgpu::Buffer ubo;

float RandomFloat(float min, float max) {
    // NOLINTNEXTLINE(runtime/threadsafe_fn)
    float zeroOne = rand() / static_cast<float>(RAND_MAX);
    return zeroOne * (max - min) + min;
}

constexpr size_t kNumTriangles = 10000;

// Aligned as minUniformBufferOffsetAlignment
struct alignas(256) ShaderData {
    float scale;
    float time;
    float offsetX;
    float offsetY;
    float scalar;
    float scalarOffset;
};

static std::vector<ShaderData> shaderData;

void init() {
    device = CreateCppDawnDevice();

    queue = device.GetQueue();
    swapchain = GetSwapChain();

    wgpu::ShaderModule vsModule = dawn::utils::CreateShaderModule(device, R"(
        struct Constants {
            scale : f32,
            time : f32,
            offsetX : f32,
            offsetY : f32,
            scalar : f32,
            scalarOffset : f32,
        };
        @group(0) @binding(0) var<uniform> c : Constants;

        struct VertexOut {
            @location(0) v_color : vec4f,
            @builtin(position) Position : vec4f,
        };

        @vertex fn main(@builtin(vertex_index) VertexIndex : u32) -> VertexOut {
            var positions : array<vec4f, 3> = array(
                vec4f( 0.0,  0.1, 0.0, 1.0),
                vec4f(-0.1, -0.1, 0.0, 1.0),
                vec4f( 0.1, -0.1, 0.0, 1.0)
            );

            var colors : array<vec4f, 3> = array(
                vec4f(1.0, 0.0, 0.0, 1.0),
                vec4f(0.0, 1.0, 0.0, 1.0),
                vec4f(0.0, 0.0, 1.0, 1.0)
            );

            var position : vec4f = positions[VertexIndex];
            var color : vec4f = colors[VertexIndex];

            // TODO(dawn:572): Revisit once modf has been reworked in WGSL.
            var fade : f32 = c.scalarOffset + c.time * c.scalar / 10.0;
            fade = fade - floor(fade);
            if (fade < 0.5) {
                fade = fade * 2.0;
            } else {
                fade = (1.0 - fade) * 2.0;
            }

            var xpos : f32 = position.x * c.scale;
            var ypos : f32 = position.y * c.scale;
            let angle : f32 = 3.14159 * 2.0 * fade;
            let xrot : f32 = xpos * cos(angle) - ypos * sin(angle);
            let yrot : f32 = xpos * sin(angle) + ypos * cos(angle);
            xpos = xrot + c.offsetX;
            ypos = yrot + c.offsetY;

            var output : VertexOut;
            output.v_color = vec4f(fade, 1.0 - fade, 0.0, 1.0) + color;
            output.Position = vec4f(xpos, ypos, 0.0, 1.0);
            return output;
        })");

    wgpu::ShaderModule fsModule = dawn::utils::CreateShaderModule(device, R"(
        @fragment fn main(@location(0) v_color : vec4f) -> @location(0) vec4f {
            return v_color;
        })");

    wgpu::BindGroupLayout bgl = dawn::utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Vertex, wgpu::BufferBindingType::Uniform, true}});

    dawn::utils::ComboRenderPipelineDescriptor descriptor;
    descriptor.layout = dawn::utils::MakeBasicPipelineLayout(device, &bgl);
    descriptor.vertex.module = vsModule;
    descriptor.cFragment.module = fsModule;
    descriptor.cTargets[0].format = GetPreferredSwapChainTextureFormat();

    pipeline = device.CreateRenderPipeline(&descriptor);

    shaderData.resize(kNumTriangles);
    for (auto& data : shaderData) {
        data.scale = RandomFloat(0.2f, 0.4f);
        data.time = 0.0;
        data.offsetX = RandomFloat(-0.9f, 0.9f);
        data.offsetY = RandomFloat(-0.9f, 0.9f);
        data.scalar = RandomFloat(0.5f, 2.0f);
        data.scalarOffset = RandomFloat(0.0f, 10.0f);
    }

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = kNumTriangles * sizeof(ShaderData);
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    ubo = device.CreateBuffer(&bufferDesc);

    bindGroup = dawn::utils::MakeBindGroup(device, bgl, {{0, ubo, 0, sizeof(ShaderData)}});
}

int frameCount = 0;
void frame() {
    wgpu::TextureView backbufferView = swapchain.GetCurrentTextureView();

    if (frameCount == 20) {
        device.Destroy();
    }

    for (auto& data : shaderData) {
        data.time = frameCount / 60.0f;
    }
    queue.WriteBuffer(ubo, 0, shaderData.data(), kNumTriangles * sizeof(ShaderData));

    dawn::utils::ComboRenderPassDescriptor renderPass({backbufferView});
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);

        for (size_t i = 0; i < kNumTriangles; i++) {
            uint32_t offset = i * sizeof(ShaderData);
            pass.SetBindGroup(0, bindGroup, 1, &offset);
            pass.Draw(3);
        }

        pass.End();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    static constexpr uint64_t kTimeout = 1000;
    static constexpr bool kUseFd = false;
    static constexpr bool kSubmitBetweenFutures = true;
    static constexpr int kNumWaits = 1;
    static constexpr int kNumFuturesPerWait = 1;
    static constexpr bool kUseThen = false;
    for (int i = 0; i < kNumWaits; ++i) {
        wgpu::QueueWorkDoneDescriptorFd descFd{};
        wgpu::QueueWorkDoneDescriptor desc{};
        if (kUseFd) {
            desc.nextInChain = &descFd;
        }
        std::vector<wgpu::QueueWorkDoneFuture> futures;
        for (int j = 0; j < kNumFuturesPerWait; ++j) {
            if (kSubmitBetweenFutures) {
                wgpu::CommandBuffer cb = device.CreateCommandEncoder().Finish();
                queue.Submit(1, &cb);
            }
            futures.push_back(queue.OnSubmittedWorkDone2(&desc));
        }
        DoFlushCmdBufs();

        if (kUseFd) {
            std::vector<int> fds(futures.size());
            wgpu::FuturesGetEarliestFds(
                futures.size(), reinterpret_cast<wgpu::Future*>(futures.data()), fds.data());

            std::vector<struct pollfd> pfds;
            for (int j = 0; j < kNumFuturesPerWait; ++j) {
                if (fds[j] != -1) {
                    pfds.push_back({fds[j], POLLIN, 0});
                }
            }
            ASSERT(pfds.size() == 1);

            while (pfds.size()) {
                int status = poll(pfds.data(), pfds.size(), kTimeout);
                ASSERT(status > 0);
                std::erase_if(pfds, [](const auto& pfd) {
                    if (pfd.revents & POLLIN) {
                        return true;
                    } else {
                        ASSERT(pfd.revents == 0);
                        return false;
                    }
                });
            }
            size_t count = futures.size();
            auto waited =
                wgpu::FuturesWaitAny(&count, reinterpret_cast<wgpu::Future*>(futures.data()), 0);
            ASSERT(waited == wgpu::WaitStatus::SomeCompleted);
            ASSERT(count == 0);
            futures.resize(count);
        } else {
            bool done = false;
            if (kUseThen) {
                futures[0].Then(
                    wgpu::CallbackMode::AllowReentrant,
                    [](WGPUQueueWorkDoneFuture, void* userdata) {
                        *static_cast<bool*>(userdata) = true;
                    },
                    &done);
            }
            size_t count = futures.size();
            wgpu::WaitStatus waited;
            while (true) {  // FIXME: remove loop
                device.Tick();
                DoFlushCmdBufs();
                waited = wgpu::FuturesWaitAny(
                    &count, reinterpret_cast<wgpu::Future*>(futures.data()), kTimeout);
                if (waited != wgpu::WaitStatus::TimedOut) {
                    break;
                }
                printf("waiting...\n");
                usleep(10'000);
            }
            if (kUseThen) {
                ASSERT(done);
            }
            ASSERT(waited == wgpu::WaitStatus::SomeCompleted);
            ASSERT(count == 0);
            futures.resize(count);
        }
    }

    swapchain.Present();
    DoFlush();
}

int main(int argc, const char* argv[]) {
    if (!InitSample(argc, argv)) {
        return 1;
    }
    init();

    dawn::utils::Timer* timer = dawn::utils::CreateTimer();
    timer->Start();
    while (!ShouldQuit()) {
        ProcessEvents();
        frameCount++;
        frame();
        if (frameCount % 60 == 0) {
            printf("FPS: %lf\n", 60.0 / timer->GetElapsedTime());
            timer->Start();
        }
    }
}
