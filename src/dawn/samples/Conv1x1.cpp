
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
#include <iostream>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include "dawn/dawn_proc.h"
#include "dawn/native/DawnNative.h"


constexpr double timestamp_period = 0.0;
constexpr bool dump_shaders = false;
constexpr uint32_t trials = 10;
constexpr uint32_t dispatches = 10;

// Tests 2D convolution 1x1x128x12288 -> 1x1x128x1536
constexpr uint32_t kSharedDim = 128;  // number of floats
constexpr uint32_t kSrcDim = 12288;   // number of floats
constexpr uint32_t kDstDim = 1536;    // number of floats
constexpr char kWGSLShader[] = R"(
@group(0) @binding(0) var dst_tensor_image2d : texture_storage_2d<rgba32float, write>;
@group(0) @binding(1) var src_tensor_image2d : texture_2d<f32>;
struct biases_buffer_vector {
  data: array<vec4<f32>>,
};
@group(0) @binding(2) var<storage, read> biases_buffer : biases_buffer_vector;
struct weights_buffer_vector {
  data: array<vec4<f32>>,
};
@group(0) @binding(3) var<storage, read> weights_buffer : weights_buffer_vector;
struct Scalars {
  i0 : vec4<i32>,
  i1 : vec4<i32>,
};
@group(0) @binding(4) var<uniform> U: Scalars;
var<workgroup> weights_cache : array<vec4f, 32>;
@compute @workgroup_size(64, 1, 1)
fn main(
  @builtin(global_invocation_id) gid : vec3<u32>,
  @builtin(workgroup_id) wid : vec3<u32>,
  @builtin(local_invocation_id) lid : vec3<u32>
) {
  var DST_X : i32 = i32(gid.x) % U.i0.w;
  var DST_Y : i32 = (i32(gid.x) / U.i0.w) % U.i1.x;
  var DST_S : i32 = i32(wid.y);
  DST_S *= 4;
  if (DST_S >= U.i0.y) { return; }
  var r_w0_h0_s0 : vec4f = vec4f(0.0f);
  var r_w0_h0_s1 : vec4f = vec4f(0.0f);
  var r_w0_h0_s2 : vec4f = vec4f(0.0f);
  var r_w0_h0_s3 : vec4f = vec4f(0.0f);
  var filters_offset : u32 = u32(DST_S * 4 * U.i0.z);
  var s : i32 = 0;
  while(true) {
    workgroupBarrier();
    if (lid.x < 32) {
      weights_cache[lid.x] = weights_buffer.data[filters_offset + lid.x];
    }
    workgroupBarrier();
    var src_w0_h0 : vec4f;
    src_w0_h0 = textureLoad(src_tensor_image2d, vec2<i32>((DST_X), ((DST_Y) * U.i0.z + (s))), 0);
    s += 1;
    r_w0_h0_s0 += weights_cache[0] * src_w0_h0.x;
    r_w0_h0_s0 += weights_cache[1] * src_w0_h0.y;
    r_w0_h0_s0 += weights_cache[2] * src_w0_h0.z;
    r_w0_h0_s0 += weights_cache[3] * src_w0_h0.w;
    r_w0_h0_s1 += weights_cache[4] * src_w0_h0.x;
    r_w0_h0_s1 += weights_cache[5] * src_w0_h0.y;
    r_w0_h0_s1 += weights_cache[6] * src_w0_h0.z;
    r_w0_h0_s1 += weights_cache[7] * src_w0_h0.w;
    r_w0_h0_s2 += weights_cache[8] * src_w0_h0.x;
    r_w0_h0_s2 += weights_cache[9] * src_w0_h0.y;
    r_w0_h0_s2 += weights_cache[10] * src_w0_h0.z;
    r_w0_h0_s2 += weights_cache[11] * src_w0_h0.w;
    r_w0_h0_s3 += weights_cache[12] * src_w0_h0.x;
    r_w0_h0_s3 += weights_cache[13] * src_w0_h0.y;
    r_w0_h0_s3 += weights_cache[14] * src_w0_h0.z;
    r_w0_h0_s3 += weights_cache[15] * src_w0_h0.w;
    src_w0_h0 = textureLoad(src_tensor_image2d, vec2<i32>((DST_X), ((DST_Y) * U.i0.z + (s))), 0);
    r_w0_h0_s0 += weights_cache[16] * src_w0_h0.x;
    r_w0_h0_s0 += weights_cache[17] * src_w0_h0.y;
    r_w0_h0_s0 += weights_cache[18] * src_w0_h0.z;
    r_w0_h0_s0 += weights_cache[19] * src_w0_h0.w;
    r_w0_h0_s1 += weights_cache[20] * src_w0_h0.x;
    r_w0_h0_s1 += weights_cache[21] * src_w0_h0.y;
    r_w0_h0_s1 += weights_cache[22] * src_w0_h0.z;
    r_w0_h0_s1 += weights_cache[23] * src_w0_h0.w;
    r_w0_h0_s2 += weights_cache[24] * src_w0_h0.x;
    r_w0_h0_s2 += weights_cache[25] * src_w0_h0.y;
    r_w0_h0_s2 += weights_cache[26] * src_w0_h0.z;
    r_w0_h0_s2 += weights_cache[27] * src_w0_h0.w;
    r_w0_h0_s3 += weights_cache[28] * src_w0_h0.x;
    r_w0_h0_s3 += weights_cache[29] * src_w0_h0.y;
    r_w0_h0_s3 += weights_cache[30] * src_w0_h0.z;
    r_w0_h0_s3 += weights_cache[31] * src_w0_h0.w;
    s += 1;
    filters_offset += 32;
    if (s >= U.i0.z) { break; }
  }
  if (DST_Y >= U.i0.x || DST_S >= U.i0.y) {
    return;
  }
  if (DST_S + 0 >= U.i0.y) { return; }
  {
    let bias_val : vec4f = biases_buffer.data[(DST_S + 0)];
  {
    let res : vec4f = r_w0_h0_s0 + bias_val;
    textureStore(dst_tensor_image2d, vec2<i32>((DST_X + 0), ((DST_Y + 0) * U.i0.y + (DST_S + 0))), res);
  }
  }
  if (DST_S + 1 >= U.i0.y) { return; }
  {
    let bias_val : vec4f = biases_buffer.data[(DST_S + 1)];
  {
    let res : vec4f = r_w0_h0_s1 + bias_val;
    textureStore(dst_tensor_image2d, vec2<i32>((DST_X + 0), ((DST_Y + 0) * U.i0.y + (DST_S + 1))), res);
  }
  }
  if (DST_S + 2 >= U.i0.y) { return; }
  {
    let bias_val : vec4f = biases_buffer.data[(DST_S + 2)];
  {
    let res : vec4f = r_w0_h0_s2 + bias_val;
    textureStore(dst_tensor_image2d, vec2<i32>((DST_X + 0), ((DST_Y + 0) * U.i0.y + (DST_S + 2))), res);
  }
  }
  if (DST_S + 3 >= U.i0.y) { return; }
  {
    let bias_val : vec4f = biases_buffer.data[(DST_S + 3)];
  {
    let res : vec4f = r_w0_h0_s3 + bias_val;
    textureStore(dst_tensor_image2d, vec2<i32>((DST_X + 0), ((DST_Y + 0) * U.i0.y + (DST_S + 3))), res);
  }
  }
}
)";

void TestWebGPU() {
    dawnProcSetProcs(&dawn::native::GetProcs());
    std::unique_ptr<dawn::native::Instance> instance = std::make_unique<dawn::native::Instance>();
    wgpu::RequestAdapterOptions adapterOptions;
    auto adapters = instance->EnumerateAdapters(&adapterOptions);
    if (!adapters.empty()) {
        auto adapter = adapters[0];
        wgpu::AdapterProperties properties;
        adapter.GetProperties(&properties);
        std::cout << "Dawn using " << properties.name << std::endl;
        std::vector<const char*> enabledToggles = {
            "allow_unsafe_apis",
            "disable_workgroup_init",
            "disable_robustness",
        };
        if (dump_shaders) {
            enabledToggles.push_back("dump_shaders");
        }
        if (timestamp_period > 0) {
            enabledToggles.push_back("disable_timestamp_query_conversion");
        }
        const char* disabledToggles[] = {
            "lazy_clear_resource_on_first_use",
        };
        wgpu::DawnTogglesDescriptor togglesDesc;
        togglesDesc.enabledToggles = enabledToggles.data();
        togglesDesc.enabledToggleCount = enabledToggles.size();
        togglesDesc.disabledToggles = disabledToggles;
        togglesDesc.disabledToggleCount = sizeof(disabledToggles) / sizeof(disabledToggles[0]);
        wgpu::DeviceDescriptor deviceDesc;
        deviceDesc.nextInChain = &togglesDesc;
        wgpu::FeatureName requiredFeatures[] = {wgpu::FeatureName::TimestampQuery};
        deviceDesc.requiredFeatures = requiredFeatures;
        deviceDesc.requiredFeatureCount = sizeof(requiredFeatures) / sizeof(requiredFeatures[0]);
        deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message,
                                           void* userdata) {
            if (reason == WGPUDeviceLostReason_Destroyed) {
                return;
            }
            if (message) {
                std::cerr << message << std::endl;
            }
        };
        auto device = wgpu::Device::Acquire(adapter.CreateDevice(&deviceDesc));
        device.SetUncapturedErrorCallback(
            [](WGPUErrorType type, char const* message, void* userdata) {
                if (message) {
                    std::cerr << message << std::endl;
                }
            },
            nullptr);
        device.SetLoggingCallback(
            [](WGPULoggingType type, char const* message, void* userdata) {
                if (message) {
                    std::cout << message << std::endl;
                }
            },
            nullptr);
        wgpu::ShaderModuleWGSLDescriptor shaderModuleWGSLDesc;
        shaderModuleWGSLDesc.code = kWGSLShader;
        wgpu::ShaderModuleDescriptor shaderModuleDesc;
        shaderModuleDesc.nextInChain = &shaderModuleWGSLDesc;
        wgpu::ShaderModule shaderModule = device.CreateShaderModule(&shaderModuleDesc);
        wgpu::ComputePipelineDescriptor pipelineDesc;
        pipelineDesc.compute.module = shaderModule;
        pipelineDesc.compute.entryPoint = "main";
        wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&pipelineDesc);
        wgpu::BufferDescriptor uniformBufferDesc;
        uniformBufferDesc.size = sizeof(int32_t) * 2 * 4;
        uniformBufferDesc.usage = wgpu::BufferUsage::Uniform;
        uniformBufferDesc.mappedAtCreation = true;
        wgpu::Buffer uniformBuffer = device.CreateBuffer(&uniformBufferDesc);
        {
            uint32_t* uniformData = static_cast<uint32_t*>(uniformBuffer.GetMappedRange());
            uniformData[0] = 1;
            uniformData[1] = kDstDim / 4;
            uniformData[2] = kSrcDim / 4;
            uniformData[3] = kSharedDim;
            uniformData[4] = 1;
            uniformData[5] = 0;
            uniformData[6] = 0;
            uniformData[7] = 0;
        }
        uniformBuffer.Unmap();
        wgpu::TextureDescriptor textureDesc;
        textureDesc.format = wgpu::TextureFormat::RGBA32Float;
        // Tensor size is divided by 4 in the Y dimension because 4 floats are packed in each texel.
        textureDesc.size = {kSharedDim, kDstDim / 4};
        textureDesc.usage = wgpu::TextureUsage::StorageBinding;
        textureDesc.label = "dstTensor";
        wgpu::Texture dstTensor = device.CreateTexture(&textureDesc);
        textureDesc.size = {kSharedDim, kSrcDim / 4};
        textureDesc.usage = wgpu::TextureUsage::TextureBinding;
        textureDesc.label = "srcTensor";
        wgpu::Texture srcTensor = device.CreateTexture(&textureDesc);
        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.usage = wgpu::BufferUsage::Storage;
        bufferDesc.size = kDstDim * sizeof(float);
        bufferDesc.label = "biasTensor";
        wgpu::Buffer biasTensor = device.CreateBuffer(&bufferDesc);
        bufferDesc.size = kDstDim * kSrcDim * sizeof(float);
        bufferDesc.label = "weightsTensor";
        wgpu::Buffer weightsTensor = device.CreateBuffer(&bufferDesc);
        wgpu::BindGroupDescriptor bindGroupDesc;
        wgpu::BindGroupEntry bindGroupEntries[5];
        bindGroupEntries[0].binding = 0;
        bindGroupEntries[0].textureView = dstTensor.CreateView();
        bindGroupEntries[1].binding = 1;
        bindGroupEntries[1].textureView = srcTensor.CreateView();
        bindGroupEntries[2].binding = 2;
        bindGroupEntries[2].buffer = biasTensor;
        bindGroupEntries[3].binding = 3;
        bindGroupEntries[3].buffer = weightsTensor;
        bindGroupEntries[4].binding = 4;
        bindGroupEntries[4].buffer = uniformBuffer;
        bindGroupDesc.layout = pipeline.GetBindGroupLayout(0);
        bindGroupDesc.entries = bindGroupEntries;
        bindGroupDesc.entryCount = sizeof(bindGroupEntries) / sizeof(bindGroupEntries[0]);
        wgpu::BindGroup bindGroup = device.CreateBindGroup(&bindGroupDesc);
        wgpu::QuerySetDescriptor querySetDesc;
        querySetDesc.type = wgpu::QueryType::Timestamp;
        querySetDesc.count = 2 * trials;
        wgpu::QuerySet querySet = device.CreateQuerySet(&querySetDesc);
        bufferDesc.size = sizeof(uint64_t) * 2 * trials;
        bufferDesc.usage = wgpu::BufferUsage::QueryResolve | wgpu::BufferUsage::CopySrc;
        bufferDesc.label = "queryResult";
        wgpu::Buffer querySetResults = device.CreateBuffer(&bufferDesc);
        bufferDesc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
        bufferDesc.label = "queryReadBack";
        wgpu::Buffer querySetReadback = device.CreateBuffer(&bufferDesc);
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        for (uint32_t i = 0; i < trials; ++i) {
            wgpu::ComputePassDescriptor computePassDesc;
            wgpu::ComputePassTimestampWrites timestampWrites;
            timestampWrites.querySet = querySet;
            timestampWrites.beginningOfPassWriteIndex = 2 * i;
            timestampWrites.endOfPassWriteIndex = 2 * i + 1;
            computePassDesc.timestampWrites = &timestampWrites;
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass(&computePassDesc);
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, bindGroup);
            for (uint32_t d = 0; d < dispatches; ++d) {
                pass.DispatchWorkgroups(kSharedDim / 64, kDstDim / 4 / 4, 1);
            }
            pass.End();
        }
        encoder.ResolveQuerySet(querySet, 0, 2 * trials, querySetResults, 0);
        encoder.CopyBufferToBuffer(querySetResults, 0, querySetReadback, 0,
                                   sizeof(uint64_t) * 2 *trials);
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
            std::this_thread::sleep_for(100ms);
            device.Tick();
        }
        const uint64_t* timestamps =
            static_cast<const uint64_t*>(querySetReadback.GetConstMappedRange());
        double min = std::numeric_limits<double>::infinity();
        double max = -1;
        double avg = 0;
        for (uint32_t i = 0; i < trials; ++i) {
            double duration =
                static_cast<double>(timestamps[2 * i + 1] - timestamps[2 * i]) * 1.0e-6;
            double period = timestamp_period;
            if (period > 0) {
                duration *= period;
            }
            if (duration < min) {
                min = duration;
            }
            if (duration > max) {
                max = duration;
            }
            avg += duration / trials;
        }
        std::cout << "Min: " << min << " ms " << std::endl;
        std::cout << "Max: " << max << " ms " << std::endl;
        std::cout << "Avg: " << avg << " ms " << std::endl;
    }
}
int main(int argc, char** argv) {
    TestWebGPU();
}
