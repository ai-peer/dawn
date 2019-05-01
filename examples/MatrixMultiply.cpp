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

#include "utils/DawnHelpers.h"
#include "utils/SystemUtils.h"

#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>

constexpr int32_t kTrials = 10;
constexpr uint32_t kMatrixSize = 500;
constexpr uint32_t kTiledTileSize = 32;
constexpr uint32_t kTiledDispatchSize = (kMatrixSize + kTiledTileSize - 1) / kTiledTileSize;
constexpr uint32_t kTileSize = 32;
constexpr uint32_t kWorkPerThread = 2;
constexpr uint32_t kDispatchSize = (kMatrixSize + kTileSize - 1) / kTileSize;
constexpr uint32_t kNumMultiplications = 100;
constexpr uint32_t kBufferSize = sizeof(float) * kMatrixSize * kMatrixSize;

dawn::Device device;
dawn::Queue queue;

void ReadbackAndWait(dawn::Buffer buffer) {
    dawn::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = kBufferSize;
    bufferDescriptor.usage = dawn::BufferUsageBit::TransferDst | dawn::BufferUsageBit::MapRead;
    dawn::Buffer result = device.CreateBuffer(&bufferDescriptor);

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToBuffer(buffer, 0, result, 0, kBufferSize);
    dawn::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);

    bool done = false;
    result.MapReadAsync(
        [](DawnBufferMapAsyncStatus status, const void* data, uint64_t size,
           DawnCallbackUserdata userdata) { *reinterpret_cast<bool*>(userdata) = true; },
        reinterpret_cast<DawnCallbackUserdata>(&done));

    while (!done) {
        device.Tick();
        DoFlush();
    }
}

template <typename Multiply>
void ProfileMultiply(dawn::Buffer A,
                 dawn::Buffer B,
                 dawn::Buffer C,
                 Multiply multiply) {
    dawn::FenceDescriptor descriptor;
    descriptor.initialValue = 0;
    dawn::Fence fence = queue.CreateFence(&descriptor);

    std::cout << kTrials << " trials of " << kNumMultiplications << " consecutive " << kMatrixSize << "x"
              << kMatrixSize << " multiplications" << std::endl;
    for (uint32_t trial = 0; trial != kTrials && !ShouldQuit(); ++trial) {

        auto start = std::chrono::high_resolution_clock::now();

        std::array<dawn::CommandBuffer, kNumMultiplications> commandBuffers;
        for (uint32_t i = 0; i < kNumMultiplications; ++i) {
            commandBuffers[i] = multiply(A, B, C);
            std::swap(A, C);
        }
        queue.Submit(commandBuffers.size(), commandBuffers.data());
        ReadbackAndWait(A);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        double ms_per_mul = static_cast<double>(duration.count()) / (1000.0);
        std::cout << ms_per_mul << " ms, "
                  << ms_per_mul / kNumMultiplications << " ms (avg)" << std::endl;
    }
}

int main(int argc, const char* argv[]) {
    if (!InitSample(argc, argv)) {
        return 1;
    }

    device = CreateCppDawnDevice();
    queue = device.CreateQueue();

    dawn::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = kBufferSize;
    bufferDescriptor.usage = dawn::BufferUsageBit::Storage | dawn::BufferUsageBit::TransferDst |
                             dawn::BufferUsageBit::TransferSrc;

    dawn::Buffer bufferA = device.CreateBuffer(&bufferDescriptor);
    dawn::Buffer bufferB = device.CreateBuffer(&bufferDescriptor);
    dawn::Buffer bufferC = device.CreateBuffer(&bufferDescriptor);

    std::array<uint32_t, 4> dimensions{kMatrixSize, kMatrixSize, kMatrixSize, 1};
    dawn::Buffer bufferDimensions = utils::CreateBufferFromData(
        device, dimensions.data(), sizeof(dimensions), dawn::BufferUsageBit::Uniform);

    std::string tileLocalShader = R"(
        #version 450

        const uint TileSize = )" +
                               std::to_string(kTiledTileSize) + R"(;

        layout(local_size_x = TileSize, local_size_y = TileSize, local_size_z = 1) in;

        layout(std430, set = 0, binding = 0) readonly buffer ssboA {
          float A[];
        };

        layout(std430, set = 0, binding = 1) readonly buffer ssboB {
          float B[];
        };

        layout(std140, set = 0, binding = 2) uniform uniformDimensions {
          uvec4 Dimensions;
        };

        layout(std430, set = 0, binding = 3) writeonly buffer ssboC {
          float C[];
        };

        shared float Asub[TileSize][TileSize + 2];
        shared float Bsub[TileSize][TileSize + 2];

        void main() {
            // M is A outer, N is shared, K is B outer
            uint M = Dimensions[0], N = Dimensions[1],
              K = Dimensions[2], batch = Dimensions[3];

            uint row = gl_LocalInvocationID.x; // Local row ID (max: TileSize)
            uint col = gl_LocalInvocationID.y; // Local col ID (max: TileSize)
            uint globalRow = TileSize*gl_WorkGroupID.x + row; // Row ID of C (0..M)
            uint globalCol = TileSize*gl_WorkGroupID.y + col; // Col ID of C (0..N)

            float acc = 0.0;

            const uint NumTiles = (N - 1)/TileSize + 1;

            for (uint t=0; t < NumTiles; t++) {
                // Load one tile of A and B into local memory
                uint tiledRow = TileSize*t + row;
                uint tiledCol = TileSize*t + col;
                Asub[col][row] = A[globalRow*N + tiledCol];
                Bsub[row][col] = B[tiledRow*K + globalCol];
                barrier();
                for (uint k=0; k<TileSize; k++) {
                    acc += Asub[k][row] * Bsub[k][col];
                }
                barrier();
            }
            if (globalCol < K && globalRow < M) {
                C[globalRow*K + globalCol] = acc;
            }
        }
    )";

    std::string registerBlockingShader = R"(
        #version 450

        const uint TileSize = )" + std::to_string(kTileSize) +
                                         R"(;

        const uint WorkPerThread = )" + std::to_string(kWorkPerThread) +
                                         R"(;

        layout(local_size_x = TileSize / WorkPerThread, local_size_y = TileSize / WorkPerThread, local_size_z = 1) in;

        layout(std430, set = 0, binding = 0) readonly buffer ssboA {
          float A[];
        };

        layout(std430, set = 0, binding = 1) readonly buffer ssboB {
          float B[];
        };

        layout(std140, set = 0, binding = 2) uniform uniformDimensions {
          uvec4 Dimensions;
        };

        layout(std430, set = 0, binding = 3) writeonly buffer ssboC {
          float C[];
        };

        shared float Asub[2][TileSize][TileSize + 1];
        shared float Bsub[2][TileSize][TileSize + 1];

        void main() {
            // M is A outer, N is shared, K is B outer
            uint M = Dimensions[0], N = Dimensions[1],
                K = Dimensions[2], batch = Dimensions[3];

            uint row = gl_LocalInvocationID.x; // 0..local_size_x
            uint col = gl_LocalInvocationID.y; // 0..local_size_y
            uint tileRow = row * WorkPerThread; // 0..TileSize, stride by local_size
            uint tileCol = col * WorkPerThread; // 0..TileSize
            uint globalRow = TileSize*gl_WorkGroupID.x + tileRow; // 0..M, stride by tileSize
            uint globalCol = TileSize*gl_WorkGroupID.y + tileCol;

            const uint NumTiles = (N - 1)/TileSize + 1;

            float acc[WorkPerThread][WorkPerThread];

            // Without this initialization strange values show up in acc.
            for(uint innerRow=0; innerRow<WorkPerThread; innerRow++) {
                for(uint innerCol=0; innerCol<WorkPerThread; innerCol++) {
                    acc[innerRow][innerCol] = 0.0;
                }
            }

            for(uint innerRow=0; innerRow < WorkPerThread; innerRow++) {
                for(uint innerCol=0; innerCol<WorkPerThread; innerCol++) {
                    uint inputRow = tileRow + innerRow;
                    uint inputCol = tileCol + innerCol;

                    uint AColumnIndex = 0 * TileSize + tileCol + innerCol;
                    uint AFlatIndex = (globalRow + innerRow) * N + AColumnIndex;
                    if (AColumnIndex < N) {
                        Asub[0][inputRow][inputCol] = A[AFlatIndex];
                    } else {
                        Asub[0][inputRow][inputCol] = 0.0;
                    }
                    uint BRowIndex = 0 * TileSize + tileRow + innerRow;
                    uint BFlatIndex = BRowIndex * K + (globalCol + innerCol);
                    if(BRowIndex < N) {
                        Bsub[0][inputRow][inputCol] = B[BFlatIndex];
                    } else {
                        Bsub[0][inputRow][inputCol] = 0.0;
                    }
                }
            }

            // Loop over shared dimension.
            for(uint t=0; t < NumTiles; t++) {
                barrier();

                // Load the next tile
                if (t < NumTiles - 1) {
                    // Load one tile of A and B into shared memory.
                    for(uint innerRow=0; innerRow < WorkPerThread; innerRow++) {
                        for(uint innerCol=0; innerCol<WorkPerThread; innerCol++) {
                            uint inputRow = tileRow + innerRow;
                            uint inputCol = tileCol + innerCol;

                            uint AColumnIndex = (t + 1) * TileSize + tileCol + innerCol;
                            uint AFlatIndex = (globalRow + innerRow) * N + AColumnIndex;
                            if (AColumnIndex < N) {
                                Asub[(t + 1) % 2][inputRow][inputCol] = A[AFlatIndex];
                            } else {
                                Asub[(t + 1) % 2][inputRow][inputCol] = 0.0;
                            }
                            uint BRowIndex = (t + 1) * TileSize + tileRow + innerRow;
                            uint BFlatIndex = BRowIndex * K + (globalCol + innerCol);
                            if(BRowIndex < N) {
                                Bsub[(t + 1) % 2][inputRow][inputCol] = B[BFlatIndex];
                            } else {
                                Bsub[(t + 1) % 2][inputRow][inputCol] = 0.0;
                            }
                        }
                    }
                }

                // Compute acc values for a single thread.
                for(uint k=0; k<TileSize; k++) {
                    float BCached[WorkPerThread];
                    for(uint inner=0; inner < WorkPerThread; inner++) {
                        BCached[inner] = Bsub[t % 2][k][tileCol + inner];
                    }

                    for(uint innerRow=0; innerRow < WorkPerThread; innerRow++) {
                        float ACached = Asub[t % 2][tileRow + innerRow][k];
                        for(uint innerCol=0; innerCol < WorkPerThread; innerCol++) {
                            acc[innerRow][innerCol] += ACached * BCached[innerCol];
                        }
                    }
                }
            }
            for (uint innerRow=0; innerRow < WorkPerThread; innerRow++) {
                for (uint innerCol=0; innerCol < WorkPerThread; innerCol++) {
                    uint globalFlatIndex = (globalRow + innerRow) * K + (globalCol + innerCol);

                    if((globalCol + innerCol) < K && (globalRow + innerRow) < M) {
                        C[globalFlatIndex] = acc[innerRow][innerCol];
                    }
                }
            }
        }
    )";

    dawn::BindGroupLayout bgl = utils::MakeBindGroupLayout(
        device, {
                    {0, dawn::ShaderStageBit::Compute, dawn::BindingType::StorageBuffer},
                    {1, dawn::ShaderStageBit::Compute, dawn::BindingType::StorageBuffer},
                    {2, dawn::ShaderStageBit::Compute, dawn::BindingType::UniformBuffer},
                    {3, dawn::ShaderStageBit::Compute, dawn::BindingType::StorageBuffer},
                });

    auto MakeComputePipeline = [&](const std::string& shaderString) {
        dawn::ShaderModule csModule = utils::CreateShaderModule(device, dawn::ShaderStage::Compute,
                                                                shaderString.c_str());

        dawn::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);

        dawn::ComputePipelineDescriptor csDesc;
        csDesc.layout = pl;

        dawn::PipelineStageDescriptor computeStage;
        computeStage.module = csModule;
        computeStage.entryPoint = "main";
        csDesc.computeStage = &computeStage;

        dawn::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

        return pipeline;
    };

    dawn::ComputePipeline tileLocalPipeline = MakeComputePipeline(tileLocalShader);
    dawn::ComputePipeline registerBlockingPipeline = MakeComputePipeline(registerBlockingShader);

    std::cout << "\nMatMul with register blocking" << std::endl;
    ProfileMultiply(
        bufferA, bufferB, bufferC,
        [&](dawn::Buffer A, dawn::Buffer B, dawn::Buffer C) {
            dawn::BindGroup bg =
                utils::MakeBindGroup(device, bgl,
                                        {
                                            {0, A, 0, kBufferSize},
                                            {1, B, 0, kBufferSize},
                                            {2, bufferDimensions, 0, 4 * sizeof(uint32_t)},
                                            {3, C, 0, kBufferSize},
                                        });

            dawn::CommandEncoder encoder = device.CreateCommandEncoder();
            dawn::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetPipeline(registerBlockingPipeline);
            pass.SetBindGroup(0, bg, 0, nullptr);
            pass.Dispatch(kDispatchSize, kDispatchSize, 1);
            pass.EndPass();
            dawn::CommandBuffer commandBuffer = encoder.Finish();
            return commandBuffer;
        });

    std::cout << "\nMatMul with Tile local memory" << std::endl;
    ProfileMultiply(
        bufferA, bufferB, bufferC,
        [&](dawn::Buffer A, dawn::Buffer B, dawn::Buffer C) {
            dawn::BindGroup bg =
                utils::MakeBindGroup(device, bgl,
                                        {
                                            {0, A, 0, kBufferSize},
                                            {1, B, 0, kBufferSize},
                                            {2, bufferDimensions, 0, 4 * sizeof(uint32_t)},
                                            {3, C, 0, kBufferSize},
                                        });

            dawn::CommandEncoder encoder = device.CreateCommandEncoder();
            dawn::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetPipeline(tileLocalPipeline);
            pass.SetBindGroup(0, bg, 0, nullptr);
            pass.Dispatch(kTiledDispatchSize, kTiledDispatchSize, 1);
            pass.EndPass();
            dawn::CommandBuffer commandBuffer = encoder.Finish();

            std::swap(bufferA, bufferC);

            return commandBuffer;
        });
}
