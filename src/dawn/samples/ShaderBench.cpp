// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "dawn/dawn_proc.h"
#include "dawn/native/DawnNative.h"
#include "dawn/webgpu_cpp.h"

uint32_t kNumTrials = 10;
uint32_t kNumDispatches = 10;

class Runner {
  private:
    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::QuerySet querySet;
    wgpu::Buffer querySetResults;
    wgpu::Buffer querySetReadback;

  public:
    Runner() {
        auto nativeInstance = std::make_unique<dawn::native::Instance>();

        instance = wgpu::Instance(nativeInstance->Get());

        // Get an adapter to create the device with.
        wgpu::RequestAdapterOptions options = {};
        auto nativeAdapter = nativeInstance->EnumerateAdapters(&options)[0];
        adapter = wgpu::Adapter(nativeAdapter.Get());

        wgpu::SupportedLimits supportedLimits;
        adapter.GetLimits(&supportedLimits);

        std::vector<wgpu::FeatureName> requiredFeatures = {wgpu::FeatureName::TimestampQuery};
        if (adapter.HasFeature(wgpu::FeatureName::ShaderF16)) {
            requiredFeatures.push_back(wgpu::FeatureName::ShaderF16);
        }
        if (adapter.HasFeature(wgpu::FeatureName::ChromiumExperimentalSubgroups)) {
            requiredFeatures.push_back(wgpu::FeatureName::ChromiumExperimentalSubgroups);
        }

        wgpu::RequiredLimits requiredLimits;
        requiredLimits.limits = supportedLimits.limits;

        // Create the device.
        wgpu::DeviceDescriptor desc = {};
        desc.requiredFeatures = requiredFeatures.data();
        desc.requiredFeatureCount = requiredFeatures.size();
        desc.requiredLimits = &requiredLimits;
        desc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message,
                                     void* userdata) {
            if (reason == WGPUDeviceLostReason_Undefined) {
                std::cerr << message << std::endl;
                abort();
            }
        };

        wgpu::DawnTogglesDescriptor togglesDesc = {};
        desc.nextInChain = &togglesDesc;
        const char* kEnabledToggles[] = {"disable_robustness"};
        togglesDesc.enabledToggles = kEnabledToggles;
        togglesDesc.enabledToggleCount = 1u;
        device = wgpu::Device::Acquire(nativeAdapter.CreateDevice(&desc));

        device.SetUncapturedErrorCallback(
            [](WGPUErrorType, char const* message, void* userdata) {
                std::cerr << message << std::endl;
                abort();
            },
            nullptr);

        device.SetLoggingCallback(
            [](WGPULoggingType type, char const* message, void* userdata) {
                if (message) {
                    std::cout << message << std::endl;
                }
            },
            nullptr);

        wgpu::QuerySetDescriptor querySetDesc;
        querySetDesc.type = wgpu::QueryType::Timestamp;
        querySetDesc.count = 2;
        querySet = device.CreateQuerySet(&querySetDesc);

        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.size = sizeof(uint64_t) * 2;
        bufferDesc.usage = wgpu::BufferUsage::QueryResolve | wgpu::BufferUsage::CopySrc;
        bufferDesc.label = "queryResult";
        querySetResults = device.CreateBuffer(&bufferDesc);

        bufferDesc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
        bufferDesc.label = "queryReadBack";
        querySetReadback = device.CreateBuffer(&bufferDesc);
    }

    wgpu::ComputePipeline CreatePipeline(std::string shader,
                                         std::vector<wgpu::ConstantEntry> constants = {}) {
        wgpu::ShaderModuleWGSLDescriptor shaderModuleWGSLDesc;
        shaderModuleWGSLDesc.code = shader.c_str();

        wgpu::ShaderModuleDescriptor shaderModuleDesc;
        shaderModuleDesc.nextInChain = &shaderModuleWGSLDesc;
        wgpu::ShaderModule shaderModule = device.CreateShaderModule(&shaderModuleDesc);

        wgpu::ComputePipelineDescriptor pipelineDesc;
        pipelineDesc.compute.module = shaderModule;
        pipelineDesc.compute.entryPoint = "main";

        pipelineDesc.compute.constants = constants.data();
        pipelineDesc.compute.constantCount = constants.size();
        wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&pipelineDesc);
        return pipeline;
    }

    uint64_t RunShader(wgpu::ComputePipeline pipeline,
                       wgpu::BindGroup bindGroup,
                       uint32_t workgroupsX,
                       uint32_t workgroupsY = 1,
                       uint32_t workgroupsZ = 1) {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassDescriptor computePassDesc;
        wgpu::ComputePassTimestampWrites timestampWrites = {
            querySet,
            2 * 0,
            2 * 0 + 1,
        };
        computePassDesc.timestampWrites = &timestampWrites;
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass(&computePassDesc);
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        for (uint32_t d = 0; d < kNumDispatches; ++d) {
            pass.DispatchWorkgroups(workgroupsX, workgroupsY, workgroupsZ);
        }
        pass.End();
        encoder.ResolveQuerySet(querySet, 0, 2, querySetResults, 0);
        encoder.CopyBufferToBuffer(querySetResults, 0, querySetReadback, 0, sizeof(uint64_t) * 2);
        wgpu::CommandBuffer commandBuffer = encoder.Finish();
        device.GetQueue().Submit(1, &commandBuffer);

        bool done = false;
        querySetReadback.MapAsync(
            wgpu::MapMode::Read, 0, wgpu::kWholeSize,
            [](WGPUBufferMapAsyncStatus status, void* userdata) {
                if (status != WGPUBufferMapAsyncStatus_Success) {
                    abort();
                }
                *static_cast<bool*>(userdata) = true;
            },
            &done);
        while (!done) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1ms);
            instance.ProcessEvents();
        }
        const uint64_t* timestamps =
            static_cast<const uint64_t*>(querySetReadback.GetConstMappedRange());
        uint64_t ns = timestamps[1] - timestamps[0];
        querySetReadback.Unmap();
        return ns;
    }

    void GlobalMemoryBufferRead() {
        for (uint32_t workgroupSize : {32, 64}) {
            for (uint32_t readSize :
                 {4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768}) {
                for (bool striped : {false, true}) {
                    std::cout << __func__ << " " << workgroupSize << "x1 " << readSize;
                    if (striped) {
                        std::cout << "-stripe";
                    } else {
                        std::cout << "-block";
                    }
                    wgpu::ComputePipeline pipeline = CreatePipeline(
                        striped ? R"(
              @group(0) @binding(0) var<storage, read_write> buf : array<f32>;

              override workgroupSize: u32;
              override readsPerThread: u32;

              @compute @workgroup_size(workgroupSize, 1, 1)
              fn main(@builtin(global_invocation_id) gid : vec3<u32>) {
                let id = gid.y * workgroupSize + gid.x;
                let offset = id;
                var acc = 0.0;
                for (var i : u32 = 0u; i < readsPerThread; i = i + 1u) {
                  acc += buf[offset + readsPerThread * i];
                }
                if (acc == 1234.5678) {
                  // Prevent DCE. Should rarely be hit in benchmark.
                  buf[offset] = acc;
                }
              }
            )"
                                : R"(
              @group(0) @binding(0) var<storage, read_write> buf : array<f32>;

              override workgroupSize: u32;
              override readsPerThread: u32;

              @compute @workgroup_size(workgroupSize, 1, 1)
              fn main(@builtin(global_invocation_id) gid : vec3<u32>) {
                let id = gid.y * workgroupSize + gid.x;
                let offset = readsPerThread * id;
                var acc = 0.0;
                for (var i : u32 = 0u; i < readsPerThread; i = i + 1u) {
                  acc += buf[offset + i];
                }
                if (acc == 1234.5678) {
                  // Prevent DCE. Should rarely be hit in benchmark.
                  buf[offset] = acc;
                }
              }
            )",
                        {{nullptr, "workgroupSize", static_cast<double>(workgroupSize)},
                         {nullptr, "readsPerThread",
                          static_cast<double>(readSize) / sizeof(uint32_t)}});

                    wgpu::SupportedLimits supportedLimits;
                    device.GetLimits(&supportedLimits);

                    uint64_t bufferSize =
                        std::min({supportedLimits.limits.maxBufferSize,
                                  supportedLimits.limits.maxStorageBufferBindingSize,
                                  uint64_t(1lu * 1024lu * 1024lu * 1024lu)});

                    wgpu::BufferDescriptor bufferDesc;
                    bufferDesc.size = bufferSize;
                    bufferDesc.usage = wgpu::BufferUsage::Storage;

                    wgpu::BindGroupEntry bindGroupEntries[1];
                    bindGroupEntries[0].binding = 0;
                    bindGroupEntries[0].buffer = device.CreateBuffer(&bufferDesc);

                    wgpu::BindGroupDescriptor bindGroupDesc;
                    bindGroupDesc.layout = pipeline.GetBindGroupLayout(0);
                    bindGroupDesc.entries = bindGroupEntries;
                    bindGroupDesc.entryCount =
                        sizeof(bindGroupEntries) / sizeof(bindGroupEntries[0]);
                    wgpu::BindGroup bindGroup = device.CreateBindGroup(&bindGroupDesc);

                    uint32_t numThreads = bufferSize / readSize;
                    uint32_t numWorkgroupsX = numThreads / workgroupSize;
                    numWorkgroupsX = std::min(
                        numWorkgroupsX, supportedLimits.limits.maxComputeWorkgroupsPerDimension);
                    uint64_t numWorkgroupsY = numThreads / (numWorkgroupsX * workgroupSize);

                    std::cout << "\t dispatch " << numWorkgroupsX << "x" << numWorkgroupsY;

                    uint64_t bytesProcessed =
                        kNumDispatches * numWorkgroupsX * numWorkgroupsY * workgroupSize * readSize;

                    uint64_t totalNs = 0;
                    for (uint32_t i = 0; i < kNumTrials; ++i) {
                        totalNs += RunShader(pipeline, bindGroup, numWorkgroupsX, numWorkgroupsY);
                    }

                    double gb =
                        static_cast<double>(bytesProcessed * kNumTrials) / (1024 * 1024 * 1024);
                    double seconds = static_cast<double>(totalNs) * 1.0e-9;

                    std::cout << "\tGB/s = " << gb / seconds << std::endl;
                }
            }
        }
    }

    void GlobalMemoryBufferWrite() {
        for (uint32_t workgroupSize : {32, 64}) {
            for (uint32_t writeSize : {4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192}) {
                for (bool striped : {false, true}) {
                    std::cout << __func__ << " " << workgroupSize << "x1 " << writeSize;
                    if (striped) {
                        std::cout << "-stripe";
                    } else {
                        std::cout << "-block";
                    }
                    wgpu::ComputePipeline pipeline = CreatePipeline(
                        striped ? R"(
              @group(0) @binding(0) var<storage, read_write> buf : array<f32>;

              override workgroupSize: u32;
              override writesPerThread: u32;

              @compute @workgroup_size(workgroupSize, 1, 1)
              fn main(@builtin(global_invocation_id) gid : vec3<u32>) {
                let id = gid.y * workgroupSize + gid.x;
                let offset = id;
                for (var i : u32 = 0u; i < writesPerThread; i = i + 1u) {
                  buf[offset + writesPerThread * i] = f32(id);
                }
              }
            )"
                                : R"(
              @group(0) @binding(0) var<storage, read_write> buf : array<f32>;

              override workgroupSize: u32;
              override writesPerThread: u32;

              @compute @workgroup_size(workgroupSize, 1, 1)
              fn main(@builtin(global_invocation_id) gid : vec3<u32>) {
                let id = gid.y * workgroupSize + gid.x;
                let offset = writesPerThread * id;
                var acc = 0.0;
                for (var i : u32 = 0u; i < writesPerThread; i = i + 1u) {
                  buf[offset + i] = f32(id);
                }
              }
            )",
                        {{nullptr, "workgroupSize", static_cast<double>(workgroupSize)},
                         {nullptr, "writesPerThread",
                          static_cast<double>(writeSize) / sizeof(uint32_t)}});

                    wgpu::SupportedLimits supportedLimits;
                    device.GetLimits(&supportedLimits);

                    uint64_t bufferSize =
                        std::min({supportedLimits.limits.maxBufferSize,
                                  supportedLimits.limits.maxStorageBufferBindingSize,
                                  uint64_t(1lu * 1024lu * 1024lu * 1024lu)});

                    wgpu::BufferDescriptor bufferDesc;
                    bufferDesc.size = bufferSize;
                    bufferDesc.usage = wgpu::BufferUsage::Storage;

                    wgpu::BindGroupEntry bindGroupEntries[1];
                    bindGroupEntries[0].binding = 0;
                    bindGroupEntries[0].buffer = device.CreateBuffer(&bufferDesc);

                    wgpu::BindGroupDescriptor bindGroupDesc;
                    bindGroupDesc.layout = pipeline.GetBindGroupLayout(0);
                    bindGroupDesc.entries = bindGroupEntries;
                    bindGroupDesc.entryCount =
                        sizeof(bindGroupEntries) / sizeof(bindGroupEntries[0]);
                    wgpu::BindGroup bindGroup = device.CreateBindGroup(&bindGroupDesc);

                    uint32_t numThreads = bufferSize / writeSize;
                    uint32_t numWorkgroupsX = numThreads / workgroupSize;
                    numWorkgroupsX = std::min(
                        numWorkgroupsX, supportedLimits.limits.maxComputeWorkgroupsPerDimension);
                    uint64_t numWorkgroupsY = numThreads / (numWorkgroupsX * workgroupSize);

                    std::cout << "\t dispatch " << numWorkgroupsX << "x" << numWorkgroupsY;

                    uint64_t bytesProcessed = kNumDispatches * numWorkgroupsX * numWorkgroupsY *
                                              workgroupSize * writeSize;

                    uint64_t totalNs = 0;
                    for (uint32_t i = 0; i < kNumTrials; ++i) {
                        totalNs += RunShader(pipeline, bindGroup, numWorkgroupsX, numWorkgroupsY);
                    }

                    double gb =
                        static_cast<double>(bytesProcessed * kNumTrials) / (1024 * 1024 * 1024);
                    double seconds = static_cast<double>(totalNs) * 1.0e-9;

                    std::cout << "\tGB/s = " << gb / seconds << std::endl;
                }
            }
        }
    }

    void SharedMemoryBufferRead() {
        wgpu::SupportedLimits supportedLimits;
        device.GetLimits(&supportedLimits);

        uint32_t workgroupMemoryBytes = supportedLimits.limits.maxComputeWorkgroupStorageSize;

        std::cout << workgroupMemoryBytes;

        for (uint32_t workgroupSize : {32, 64}) {
            for (uint32_t readSize : {4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192}) {
                if (readSize * workgroupSize > workgroupMemoryBytes) {
                    continue;
                }

                std::cout << __func__ << " " << workgroupSize << "x1 ";

                uint32_t readsPerThread = workgroupMemoryBytes / (workgroupSize * readSize);

                std::cout << " " << readsPerThread << "x " << readSize << "-byte ";

                wgpu::ComputePipeline pipeline = CreatePipeline(
                    R"(
            @group(0) @binding(0) var<storage, read_write> out : f32;

            override workgroupSize: u32;
            override workgroupMemoryBytes: u32;
            override readsPerThread: u32;
            override readWidth: u32;

            var<workgroup> buf : array<f32, workgroupMemoryBytes / 4u>;

            @compute @workgroup_size(workgroupSize, 1, 1)
            fn main(@builtin(local_invocation_index) lid : u32) {
              var offset = readWidth * workgroupSize * lid;

              var acc = 0.0;
              for (var i : u32 = 0u; i < readsPerThread; i = i + 1u) {
                for (var j : u32 = 0u; j < readWidth; j = j + 1u) {
                  acc += buf[offset + j];
                }
                offset += readWidth * workgroupSize;
              }

              if (acc == 1234.5678) {
                // Prevent DCE. Should rarely/never be hit in benchmark.
                out = acc;
              }
            }
          )",
                    {{nullptr, "workgroupSize", static_cast<double>(workgroupSize)},
                     {nullptr, "workgroupMemoryBytes", static_cast<double>(workgroupMemoryBytes)},
                     {nullptr, "readsPerThread", static_cast<double>(readsPerThread)},
                     {nullptr, "readWidth", static_cast<double>(readSize) / sizeof(uint32_t)}});

                wgpu::BufferDescriptor bufferDesc;
                bufferDesc.size = sizeof(uint32_t);
                bufferDesc.usage = wgpu::BufferUsage::Storage;

                wgpu::BindGroupEntry bindGroupEntries[1];
                bindGroupEntries[0].binding = 0;
                bindGroupEntries[0].buffer = device.CreateBuffer(&bufferDesc);

                wgpu::BindGroupDescriptor bindGroupDesc;
                bindGroupDesc.layout = pipeline.GetBindGroupLayout(0);
                bindGroupDesc.entries = bindGroupEntries;
                bindGroupDesc.entryCount = sizeof(bindGroupEntries) / sizeof(bindGroupEntries[0]);
                wgpu::BindGroup bindGroup = device.CreateBindGroup(&bindGroupDesc);

                uint32_t numWorkgroups = 8192;

                uint64_t bytesProcessed = kNumDispatches * workgroupMemoryBytes * numWorkgroups;

                uint64_t totalNs = 0;
                for (uint32_t i = 0; i < kNumTrials; ++i) {
                    totalNs += RunShader(pipeline, bindGroup, numWorkgroups);
                }

                double gb = static_cast<double>(bytesProcessed * kNumTrials) / (1024 * 1024 * 1024);
                double seconds = static_cast<double>(totalNs) * 1.0e-9;

                std::cout << "\tGB/s = " << gb / seconds << std::endl;
            }
        }
    }

    void SharedMemoryBufferWrite() {
        wgpu::SupportedLimits supportedLimits;
        device.GetLimits(&supportedLimits);

        uint32_t workgroupMemoryBytes = supportedLimits.limits.maxComputeWorkgroupStorageSize;

        std::cout << workgroupMemoryBytes;

        for (uint32_t workgroupSize : {32, 64}) {
            for (uint32_t readSize : {4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192}) {
                if (readSize * workgroupSize > workgroupMemoryBytes) {
                    continue;
                }

                std::cout << __func__ << " " << workgroupSize << "x1 ";

                uint32_t readsPerThread = workgroupMemoryBytes / (workgroupSize * readSize);

                std::cout << " " << readsPerThread << "x " << readSize << "-byte ";

                wgpu::ComputePipeline pipeline = CreatePipeline(
                    R"(
            @group(0) @binding(0) var<storage, read_write> out : f32;

            override workgroupSize: u32;
            override workgroupMemoryBytes: u32;
            override readsPerThread: u32;
            override readWidth: u32;

            var<workgroup> buf : array<f32, workgroupMemoryBytes / 4u>;

            @compute @workgroup_size(workgroupSize, 1, 1)
            fn main(@builtin(local_invocation_index) lid : u32) {
              var offset = readWidth * workgroupSize * lid;

              for (var i : u32 = 0u; i < readsPerThread; i = i + 1u) {
                for (var j : u32 = 0u; j < readWidth; j = j + 1u) {
                  buf[offset + j] = f32(lid);
                }
                offset += readWidth * workgroupSize;
              }

              if (buf[lid] == 1234.5678) {
                // Prevent DCE. Should rarely/never be hit in benchmark.
                out = buf[lid];
              }
            }
          )",
                    {{nullptr, "workgroupSize", static_cast<double>(workgroupSize)},
                     {nullptr, "workgroupMemoryBytes", static_cast<double>(workgroupMemoryBytes)},
                     {nullptr, "readsPerThread", static_cast<double>(readsPerThread)},
                     {nullptr, "readWidth", static_cast<double>(readSize) / sizeof(uint32_t)}});

                wgpu::BufferDescriptor bufferDesc;
                bufferDesc.size = sizeof(uint32_t);
                bufferDesc.usage = wgpu::BufferUsage::Storage;

                wgpu::BindGroupEntry bindGroupEntries[1];
                bindGroupEntries[0].binding = 0;
                bindGroupEntries[0].buffer = device.CreateBuffer(&bufferDesc);

                wgpu::BindGroupDescriptor bindGroupDesc;
                bindGroupDesc.layout = pipeline.GetBindGroupLayout(0);
                bindGroupDesc.entries = bindGroupEntries;
                bindGroupDesc.entryCount = sizeof(bindGroupEntries) / sizeof(bindGroupEntries[0]);
                wgpu::BindGroup bindGroup = device.CreateBindGroup(&bindGroupDesc);

                uint32_t numWorkgroups = 8192;

                uint64_t bytesProcessed = kNumDispatches * workgroupMemoryBytes * numWorkgroups;

                uint64_t totalNs = 0;
                for (uint32_t i = 0; i < kNumTrials; ++i) {
                    totalNs += RunShader(pipeline, bindGroup, numWorkgroups);
                }

                double gb = static_cast<double>(bytesProcessed * kNumTrials) / (1024 * 1024 * 1024);
                double seconds = static_cast<double>(totalNs) * 1.0e-9;

                std::cout << "\tGB/s = " << gb / seconds << std::endl;
            }
        }
    }
};

int main() {
    dawnProcSetProcs(&dawn::native::GetProcs());

    Runner runner;
    runner.GlobalMemoryBufferRead();
    runner.GlobalMemoryBufferWrite();
    runner.SharedMemoryBufferRead();
    runner.SharedMemoryBufferWrite();
    return 0;
}
