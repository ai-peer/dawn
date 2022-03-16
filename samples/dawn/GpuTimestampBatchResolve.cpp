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

#include "SampleUtils.h"

#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/ScopedAutoreleasePool.h"
#include "dawn/utils/SystemUtils.h"
#include "dawn/utils/WGPUHelpers.h"

#include <array>
#include <cstring>
#include <random>

wgpu::Device device;
wgpu::Queue queue;
wgpu::Buffer modelBuffer;
std::array<wgpu::Buffer, 2> particleBuffers;
wgpu::Buffer updateParams;
wgpu::ComputePipeline updatePipeline;
std::array<wgpu::BindGroup, 2> updateBGs;

size_t pingpong = 0;

static const uint32_t kNumParticles = 1000;

struct Particle {
    std::array<float, 2> pos;
    std::array<float, 2> vel;
};

struct SimParams {
    float deltaT;
    float rule1Distance;
    float rule2Distance;
    float rule3Distance;
    float rule1Scale;
    float rule2Scale;
    float rule3Scale;
    int particleCount;
};

void initBuffers() {
    std::array<std::array<float, 2>, 3> model = {{
        {-0.01, -0.02},
        {0.01, -0.02},
        {0.00, 0.02},
    }};
    modelBuffer =
        utils::CreateBufferFromData(device, &model, sizeof(model), wgpu::BufferUsage::Vertex);

    SimParams params = {0.04f, 0.1f, 0.025f, 0.025f, 0.02f, 0.05f, 0.005f, kNumParticles};
    updateParams =
        utils::CreateBufferFromData(device, &params, sizeof(params), wgpu::BufferUsage::Uniform);

    std::vector<Particle> initialParticles(kNumParticles);
    {
        std::mt19937 generator;
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (auto& p : initialParticles) {
            p.pos = {dist(generator), dist(generator)};
            p.vel = {dist(generator) * 0.1f, dist(generator) * 0.1f};
        }
    }

    for (size_t i = 0; i < 2; i++) {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = sizeof(Particle) * kNumParticles;
        descriptor.usage =
            wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex | wgpu::BufferUsage::Storage;
        particleBuffers[i] = device.CreateBuffer(&descriptor);

        queue.WriteBuffer(particleBuffers[i], 0,
                          reinterpret_cast<uint8_t*>(initialParticles.data()),
                          sizeof(Particle) * kNumParticles);
    }
}

void initSim() {
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        struct Particle {
            pos : vec2<f32>;
            vel : vec2<f32>;
        };
        struct SimParams {
            deltaT : f32;
            rule1Distance : f32;
            rule2Distance : f32;
            rule3Distance : f32;
            rule1Scale : f32;
            rule2Scale : f32;
            rule3Scale : f32;
            particleCount : u32;
        };
        struct Particles {
            particles : array<Particle>;
        };
        @binding(0) @group(0) var<uniform> params : SimParams;
        @binding(1) @group(0) var<storage, read_write> particlesA : Particles;
        @binding(2) @group(0) var<storage, read_write> particlesB : Particles;

        // https://github.com/austinEng/Project6-Vulkan-Flocking/blob/master/data/shaders/computeparticles/particle.comp
        @stage(compute) @workgroup_size(1)
        fn main(@builtin(global_invocation_id) GlobalInvocationID : vec3<u32>) {
            var index : u32 = GlobalInvocationID.x;
            if (index >= params.particleCount) {
                return;
            }
            var vPos : vec2<f32> = particlesA.particles[index].pos;
            var vVel : vec2<f32> = particlesA.particles[index].vel;
            var cMass : vec2<f32> = vec2<f32>(0.0, 0.0);
            var cVel : vec2<f32> = vec2<f32>(0.0, 0.0);
            var colVel : vec2<f32> = vec2<f32>(0.0, 0.0);
            var cMassCount : u32 = 0u;
            var cVelCount : u32 = 0u;
            var pos : vec2<f32>;
            var vel : vec2<f32>;

            for (var i : u32 = 0u; i < params.particleCount; i = i + 1u) {
                if (i == index) {
                    continue;
                }

                pos = particlesA.particles[i].pos.xy;
                vel = particlesA.particles[i].vel.xy;
                if (distance(pos, vPos) < params.rule1Distance) {
                    cMass = cMass + pos;
                    cMassCount = cMassCount + 1u;
                }
                if (distance(pos, vPos) < params.rule2Distance) {
                    colVel = colVel - (pos - vPos);
                }
                if (distance(pos, vPos) < params.rule3Distance) {
                    cVel = cVel + vel;
                    cVelCount = cVelCount + 1u;
                }
            }

            if (cMassCount > 0u) {
                cMass = (cMass / vec2<f32>(f32(cMassCount), f32(cMassCount))) - vPos;
            }

            if (cVelCount > 0u) {
                cVel = cVel / vec2<f32>(f32(cVelCount), f32(cVelCount));
            }
            vVel = vVel + (cMass * params.rule1Scale) + (colVel * params.rule2Scale) +
                (cVel * params.rule3Scale);

            // clamp velocity for a more pleasing simulation
            vVel = normalize(vVel) * clamp(length(vVel), 0.0, 0.1);
            // kinematic update
            vPos = vPos + (vVel * params.deltaT);

            // Wrap around boundary
            if (vPos.x < -1.0) {
                vPos.x = 1.0;
            }
            if (vPos.x > 1.0) {
                vPos.x = -1.0;
            }
            if (vPos.y < -1.0) {
                vPos.y = 1.0;
            }
            if (vPos.y > 1.0) {
                vPos.y = -1.0;
            }

            // Write back
            particlesB.particles[index].pos = vPos;
            particlesB.particles[index].vel = vVel;
            return;
        }
    )");

    auto bgl = utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},
                    {1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},
                    {2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},
                });

    wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.layout = pl;
    csDesc.compute.module = module;
    csDesc.compute.entryPoint = "main";
    updatePipeline = device.CreateComputePipeline(&csDesc);

    for (uint32_t i = 0; i < 2; ++i) {
        updateBGs[i] = utils::MakeBindGroup(
            device, bgl,
            {
                {0, updateParams, 0, sizeof(SimParams)},
                {1, particleBuffers[i], 0, kNumParticles * sizeof(Particle)},
                {2, particleBuffers[(i + 1) % 2], 0, kNumParticles * sizeof(Particle)},
            });
    }
}

wgpu::Buffer CreateResolveBuffer(uint64_t size) {
    wgpu::BufferDescriptor descriptor;
    descriptor.size = size;
    descriptor.usage =
        wgpu::BufferUsage::QueryResolve | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    return device.CreateBuffer(&descriptor);
}

wgpu::QuerySet CreateQuerySetForTimestamp(uint32_t queryCount) {
    wgpu::QuerySetDescriptor descriptor;
    descriptor.count = queryCount;
    descriptor.type = wgpu::QueryType::Timestamp;
    return device.CreateQuerySet(&descriptor);
}

wgpu::Buffer queryBuffer;
wgpu::QuerySet querySet;
wgpu::CommandEncoder encoder;

constexpr uint32_t repeat = 100;

constexpr uint32_t kQueryCount = 2 * repeat;
int querySetIndex = 0;
int gpuFreqGlobal = 19200000;

wgpu::ComputePassEncoder pass;

wgpu::CommandBuffer createCommandBuffer(const wgpu::TextureView backbufferView, size_t i) {
    encoder = device.CreateCommandEncoder();
    pass = encoder.BeginComputePass();

    pass.WriteTimestamp(querySet, querySetIndex++);
    pass.SetPipeline(updatePipeline);
    pass.SetBindGroup(0, updateBGs[i]);
    pass.Dispatch(kNumParticles);
    pass.WriteTimestamp(querySet, querySetIndex++);
    pass.End();

    return encoder.Finish();
}

void initQuery() {
    querySet = CreateQuerySetForTimestamp(kQueryCount);
    queryBuffer = CreateResolveBuffer(kQueryCount * sizeof(uint64_t));
    return;
}

void resolveAllQuery() {
    encoder = device.CreateCommandEncoder();
    encoder.ResolveQuerySet(querySet, 0, kQueryCount, queryBuffer, 0);
    encoder.Finish();
    return;
}

void init() {
    device = CreateCppDawnHeadlessDevice();

    queue = device.GetQueue();
    initBuffers();
    initSim();
}

void WaitABit(wgpu::Device device) {
    device.Tick();
    DoHeadlessFlush();
}

static void MapCallback(WGPUBufferMapAsyncStatus status, void* userdata) {
    *static_cast<bool*>(userdata) = true;
}

void queryGPUTimeRaw() {
    int querySetSize = querySetIndex / 2;
    // The size of two query items is 16.
    int querySetBufferSize = querySetSize * 16;
    // Read data to Query Buffer.
    wgpu::BufferDescriptor descriptorA;
    descriptorA.size = querySetBufferSize;
    descriptorA.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    wgpu::Buffer bufferCPU = device.CreateBuffer(&descriptorA);

    wgpu::CommandEncoder copyencoder = device.CreateCommandEncoder();
    copyencoder.ResolveQuerySet(querySet, 0, kQueryCount, queryBuffer, 0);
    copyencoder.CopyBufferToBuffer(queryBuffer, 0, bufferCPU, 0, querySetBufferSize);
    wgpu::CommandBuffer commands = copyencoder.Finish();
    fprintf(stderr, "Copy Queue 1 %d\n", __LINE__);

    queue.Submit(1, &commands);
    DoHeadlessFlush();
    // fprintf(stderr, "Copy Queue 2 %d\n", __LINE__);
    bool done = false;
    bufferCPU.MapAsync(wgpu::MapMode::Read, 0, querySetBufferSize, MapCallback, &done);
    // fprintf(stderr, "Unknown Queue 1 %d\n", __LINE__);

    while (!done) {
        WaitABit(device);
    }

    DoHeadlessFlush();

    std::vector<uint64_t> myData(repeat*2, 0);

    fprintf(stderr, "Unknown Queue 1 %d, sizeof(myData)=%llu\n", __LINE__, myData.size());
    const int sizeOfdata = myData.size() * sizeof(uint64_t);
    memcpy(myData.data(), bufferCPU.GetConstMappedRange(), sizeOfdata);

    bufferCPU.Unmap();
    // Original is second. 1000 to ms.
    // double timeConv = 1000.0 / 12000048.0;
    // double gpuTimestampStart = (double)myData[0] * timeConv;
    // double gpuTimestampEnd = (double)myData[1] * timeConv;
    // fprintf(stderr, "GPU Timestamp(start):  %f, %f\n", (double)myData[0] * 1000.0 / 12000048.0,
    // (double)myData[1] * 1000.0 / 12000048.0);
    double gpuFreq = (double)gpuFreqGlobal;
	fprintf(stderr, "GPU Freq(Used): %f. Make sure this match the one from QueueSubmitImpl.\n",gpuFreq);
    for (uint32_t i = 0; i < repeat; i++) {
        fprintf(stderr, "GPU Timestamp(start):  %f, %f\n",
                (double)myData[i * 2] * 1000.0 / 12000048,
                (double)myData[i * 2 + 1] * 1000.0 / 12000048);
    }

    DoHeadlessFlush();
}

void frame() {
    // Execute compute.
    wgpu::CommandBuffer commandBuffer = createCommandBuffer(nullptr, pingpong);
    queue.Submit(1, &commandBuffer);
    DoHeadlessFlush();

    pingpong = (pingpong + 1) % 2;
}

int getFreq(int argc, const char** argv) {
    int gpuFreq = 19200000;
    for (int i = 1; i < argc; i++) {
        if (std::string("-f") == argv[i]) {
            i++;
            gpuFreq = atoi(argv[i]);
            fprintf(stderr,
                    "GPU Freq(Used): %d. Make sure this match the one from QueueSubmitImpl.\n",
                    gpuFreq);
            return gpuFreq;
        }
    }
    fprintf(stderr, "GPU Freq(Used): %d. Make sure this match the one from QueueSubmitImpl.\n",
            gpuFreq);
    return gpuFreq;
}

int main(int argc, const char* argv[]) {
    if (!InitSample(argc, argv)) {
        return 1;
    }
    init();
	gpuFreqGlobal= getFreq(argc, argv);
    initQuery();
    uint32_t i = 0;
    while (i < repeat) {
        i++;
        utils::ScopedAutoreleasePool pool;
        frame();
        // utils::USleep(10000);
    }
    queryGPUTimeRaw();
    // TODO: release.
}
