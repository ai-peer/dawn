// Copyright 2022 The Dawn Authors
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

#include <list>
#include <strstream>
#include <vector>

#include "dawn/tests/DawnTest.h"
#include "dawn/tests/ParamGenerator.h"
#include "dawn/utils/WGPUHelpers.h"

enum class BufferType {
    StorageBuffer,
    UniformBuffer,
};

using SrcBufferType = BufferType;

std::ostream& operator<<(std::ostream& o, BufferType bufferType) {
    switch (bufferType) {
        case BufferType::UniformBuffer:
            o << "uniform";
            break;
        case BufferType::StorageBuffer:
            o << "storage";
            break;
    }
    return o;
}

struct MemoryLayout {
    enum class SegmentType {
        data,
        padding,
    };
    struct segment {
        MemoryLayout::SegmentType type;
        size_t length;
    };
    std::list<segment> layout;
    size_t totalSize = 0;

    MemoryLayout& AddDataSegment(size_t size) {
        if (size != 0) {
            if (!layout.empty() && layout.back().type == MemoryLayout::SegmentType::data) {
                layout.back().length += size;
            } else {
                layout.push_back({MemoryLayout::SegmentType::data, size});
            }
            totalSize += size;
        }
        return *this;
    }

    MemoryLayout& AddPaddingSegment(size_t size) {
        if (size != 0) {
            if (!layout.empty() && layout.back().type == MemoryLayout::SegmentType::padding) {
                layout.back().length += size;
            } else {
                layout.push_back({MemoryLayout::SegmentType::padding, size});
            }
            totalSize += size;
        }
        return *this;
    }

    MemoryLayout& AlignTo(size_t align) {
        AddPaddingSegment((align - (totalSize % align)) % align);
        return *this;
    }

    MemoryLayout& AddingScalarOrVector(size_t size, size_t align = 0) {
        if (align == 0) {
            align = size;
        }
        AlignTo(align);
        AddDataSegment(size);
        return *this;
    }

    MemoryLayout& AddingMatrix(size_t col, size_t col_size, size_t col_align) {
        for (size_t i = 0; i < col; i++) {
            AddingScalarOrVector(col_size, col_align);
        }
        return *this;
    }

    template <class Fn>
    MemoryLayout& Repeat(uint32_t times, Fn fn) {
        for (uint32_t i = 0; i < times; i++) {
            fn(*this);
        }
        return *this;
    }

    std::vector<uint8_t> GetTestingBytes(uint8_t dataXor) const {
        size_t generatedSize = 0;
        std::vector<uint8_t> result(totalSize);
        uint8_t dataByte = 0;
        uint8_t paddingByte = 2;
        for (auto& segment : layout) {
            for (size_t i = 0; i < segment.length; i++) {
                if (segment.type == MemoryLayout::SegmentType::data) {
                    dataByte += 0x11u;
                    result[generatedSize] = dataByte ^ dataXor;
                } else {
                    paddingByte += 0x15u;
                    result[generatedSize] = paddingByte;
                }
                generatedSize++;
            }
        }
        ASSERT(generatedSize == totalSize);
        return result;
    }
};

template <typename Params = AdapterTestParam>
class ComputeBufferReadWriteTests : public DawnTestWithParams<Params> {
  public:
    wgpu::Device& device = DawnTestBase::device;
    wgpu::Queue& queue = DawnTestBase::queue;

    void WholeAssignTest(const char* shader,
                         BufferType srcBufferType,
                         MemoryLayout srcMemoryLayout,
                         MemoryLayout dstMemoryLayout) {
        // Set up shader and pipeline
        auto module = utils::CreateShaderModule(device, shader);

        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.compute.module = module;
        csDesc.compute.entryPoint = "main";

        wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

        // Set up src storage buffer
        wgpu::BufferDescriptor srcDesc;
        srcDesc.size = srcMemoryLayout.totalSize;
        srcDesc.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
        switch (srcBufferType) {
            case BufferType::StorageBuffer: {
                srcDesc.usage = srcDesc.usage | wgpu::BufferUsage::Storage;
                break;
            }
            case BufferType::UniformBuffer: {
                srcDesc.usage = srcDesc.usage | wgpu::BufferUsage::Uniform;
                break;
            }
        }
        wgpu::Buffer src = device.CreateBuffer(&srcDesc);

        auto srcData = srcMemoryLayout.GetTestingBytes(0u);
        queue.WriteBuffer(src, 0, srcData.data(), srcData.size());
        EXPECT_BUFFER_U8_RANGE_EQ(srcData.data(), src, 0, srcData.size());

        // Set up dst storage buffer
        wgpu::BufferDescriptor dstDesc;
        dstDesc.size = dstMemoryLayout.totalSize;
        dstDesc.usage =
            wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
        wgpu::Buffer dst = device.CreateBuffer(&dstDesc);

        auto dstInitData = dstMemoryLayout.GetTestingBytes(0xffu);
        auto dstExpectation = dstMemoryLayout.GetTestingBytes(0u);
        queue.WriteBuffer(dst, 0, dstInitData.data(), dstInitData.size());

        // Set up bind group and issue dispatch
        wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                         {
                                                             {0, src, 0, srcMemoryLayout.totalSize},
                                                             {1, dst, 0, dstMemoryLayout.totalSize},
                                                         });

        wgpu::CommandBuffer commands;
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, bindGroup);
            pass.DispatchWorkgroups(1);
            pass.End();

            commands = encoder.Finish();
        }

        queue.Submit(1, &commands);

        EXPECT_BUFFER_U8_RANGE_EQ(dstExpectation.data(), dst, 0, dstExpectation.size());
    }

    void WholeAssignTest(const char* shader, BufferType srcBufferType, MemoryLayout memoryLayout) {
        WholeAssignTest(shader, srcBufferType, memoryLayout, memoryLayout);
    }
};

enum class ScalarType {
    f32,
    i32,
    u32,
    f16,
};

std::ostream& operator<<(std::ostream& o, ScalarType scalar) {
    switch (scalar) {
        case ScalarType::f32:
            o << "f32";
            break;
        case ScalarType::i32:
            o << "i32";
            break;
        case ScalarType::u32:
            o << "u32";
            break;
        case ScalarType::f16:
            o << "f16";
            break;
    }
    return o;
}

DAWN_TEST_PARAM_STRUCT(BufferReadWriteScalarParams, SrcBufferType, ScalarType);

using ComputeBufferReadWriteTest_Scalar = ComputeBufferReadWriteTests<BufferReadWriteScalarParams>;

TEST_P(ComputeBufferReadWriteTest_Scalar, Plain_Scalar) {
    auto params = GetParam();
    auto srcBufferType = params.mSrcBufferType;
    auto scalar = params.mScalarType;
    size_t size = (scalar == ScalarType::f16) ? 2 : 4;
    auto layout = MemoryLayout().AddingScalarOrVector(size, size);
    std::stringstream code;
    if (scalar == ScalarType::f16) {
        code << R"(
        enable f16;
)";
    }
    code << R"(
        @group(0) @binding(0) var<)"
         << srcBufferType << R"(> src : )" << scalar << R"(;
        @group(0) @binding(1) var<storage, read_write> dst : )"
         << scalar << R"(;

        @compute @workgroup_size(1)
        fn main() {
            dst = src;
        })";
    WholeAssignTest(code.str().c_str(), srcBufferType, layout);
}

struct VectorParam {
    uint32_t n;
    ScalarType type;
};

std::ostream& operator<<(std::ostream& o, VectorParam param) {
    o << "vec" << param.n << "<" << param.type << ">";
    return o;
}

DAWN_TEST_PARAM_STRUCT(BufferReadWriteVectorParams, SrcBufferType, VectorParam);

using ComputeBufferReadWriteTest_Vector = ComputeBufferReadWriteTests<BufferReadWriteVectorParams>;

TEST_P(ComputeBufferReadWriteTest_Vector, Plain_Vec) {
    auto params = GetParam();
    auto srcBufferType = params.mSrcBufferType;
    auto vector = params.mVectorParam;
    int n = vector.n;
    auto type = vector.type;
    size_t elementSize = (type == ScalarType::f16) ? 2 : 4;
    size_t vectorSize = elementSize * n;
    size_t vectorAlign = (n == 3 ? 4 : n) * elementSize;
    auto layout = MemoryLayout().AddingScalarOrVector(vectorSize, vectorAlign);
    std::stringstream code;
    if (type == ScalarType::f16) {
        code << R"(
        enable f16;
)";
    }
    code << R"(
        @group(0) @binding(0) var<)"
         << srcBufferType << R"(> src : )" << vector << R"(;
        @group(0) @binding(1) var<storage, read_write> dst : )"
         << vector << R"(;

        @compute @workgroup_size(1)
        fn main() {
            dst = src;
        })";
    WholeAssignTest(code.str().c_str(), srcBufferType, layout);
}

TEST_P(ComputeBufferReadWriteTest_Vector, Array_Vec) {
    auto params = GetParam();
    auto srcBufferType = params.mSrcBufferType;
    auto vector = params.mVectorParam;
    int n = vector.n;
    auto type = vector.type;
    size_t elementSize = (type == ScalarType::f16) ? 2 : 4;
    size_t vectorSize = elementSize * n;
    size_t vectorAlign = (n == 3 ? 4 : n) * elementSize;
    // WGSL require array elements align to 16 in uniform buffer.
    if (srcBufferType == BufferType::UniformBuffer && vectorAlign % 16 != 0) {
        return;
    }
    auto layout =
        MemoryLayout()  //
            .Repeat(5, [=](MemoryLayout& l) { l.AddingScalarOrVector(vectorSize, vectorAlign); })
            // Size of array is rounded up to it elements' align
            .AlignTo(vectorAlign);
    std::stringstream code;
    if (type == ScalarType::f16) {
        code << R"(
        enable f16;
)";
    }
    code << R"(
        @group(0) @binding(0) var<)"
         << srcBufferType << R"(> src : array<)" << vector << R"(, 5>;
        @group(0) @binding(1) var<storage, read_write> dst : array<)"
         << vector << R"(, 5>;

        @compute @workgroup_size(1)
        fn main() {
            dst = src;
        })";
    WholeAssignTest(code.str().c_str(), srcBufferType, layout);
}

struct MatrixParam {
    uint32_t col;
    uint32_t row;
    ScalarType type;
};

std::ostream& operator<<(std::ostream& o, MatrixParam param) {
    o << "mat" << param.col << "x" << param.row << "<" << param.type << ">";
    return o;
}

DAWN_TEST_PARAM_STRUCT(BufferReadWriteMatrixParams, SrcBufferType, MatrixParam);

using ComputeBufferReadWriteTest_Matrix = ComputeBufferReadWriteTests<BufferReadWriteMatrixParams>;

TEST_P(ComputeBufferReadWriteTest_Matrix, Plain_Mat) {
    auto params = GetParam();
    auto srcBufferType = params.mSrcBufferType;
    auto matrix = params.mMatrixParam;
    auto type = matrix.type;
    int row = matrix.row;
    int col = matrix.col;
    size_t elementSize = (type == ScalarType::f16) ? 2 : 4;
    size_t colVectorSize = elementSize * row;
    size_t colVectorAlign = (row == 3 ? 4 : row) * elementSize;
    auto layout =
        MemoryLayout().AddingMatrix(col, colVectorSize, colVectorAlign).AlignTo(colVectorAlign);
    std::stringstream code;
    if (type == ScalarType::f16) {
        code << R"(
        enable f16;
)";
    }
    code << R"(
        @group(0) @binding(0) var<)"
         << srcBufferType << R"(> src : )" << matrix << R"(;
        @group(0) @binding(1) var<storage, read_write> dst : )"
         << matrix << R"(;

        @compute @workgroup_size(1)
        fn main() {
            dst = src;
        })";
    WholeAssignTest(code.str().c_str(), srcBufferType, layout);
}

TEST_P(ComputeBufferReadWriteTest_Matrix, Array_Mat) {
    auto params = GetParam();
    auto srcBufferType = params.mSrcBufferType;
    auto matrix = params.mMatrixParam;
    auto type = matrix.type;
    int row = matrix.row;
    int col = matrix.col;
    size_t elementSize = (type == ScalarType::f16) ? 2 : 4;
    size_t colVectorSize = elementSize * row;
    size_t colVectorAlign = (row == 3 ? 4 : row) * elementSize;
    // WGSL require array elements align to 16 in uniform buffer.
    if (srcBufferType == BufferType::UniformBuffer && (colVectorAlign * col) % 16 != 0) {
        return;
    }
    auto layout =
        MemoryLayout()  //
            .Repeat(5, [=](MemoryLayout& l) { l.AddingMatrix(col, colVectorSize, colVectorAlign); })
            // Size of array is rounded up to it elements' align
            .AlignTo(colVectorAlign);
    std::stringstream code;
    if (type == ScalarType::f16) {
        code << R"(
        enable f16;
)";
    }
    code << R"(
        @group(0) @binding(0) var<)"
         << srcBufferType << R"(> src : array<)" << matrix << R"(, 5>;
        @group(0) @binding(1) var<storage, read_write> dst : array<)"
         << matrix << R"(, 5>;

        @compute @workgroup_size(1)
        fn main() {
            dst = src;
        })";
    WholeAssignTest(code.str().c_str(), srcBufferType, layout);
}

DAWN_INSTANTIATE_TEST_P(ComputeBufferReadWriteTest_Scalar,
                        {D3D12Backend(), MetalBackend(), OpenGLBackend(), OpenGLESBackend(),
                         VulkanBackend()},
                        {SrcBufferType::UniformBuffer, SrcBufferType::StorageBuffer},
                        {
                            ScalarType::f32,
                            ScalarType::i32,
                            ScalarType::u32,
                        });

DAWN_INSTANTIATE_TEST_P(ComputeBufferReadWriteTest_Vector,
                        {D3D12Backend(), MetalBackend(), OpenGLBackend(), OpenGLESBackend(),
                         VulkanBackend()},
                        {SrcBufferType::UniformBuffer, SrcBufferType::StorageBuffer},
                        {
                            VectorParam{2, ScalarType::f32},
                            VectorParam{3, ScalarType::f32},
                            VectorParam{4, ScalarType::f32},
                            VectorParam{2, ScalarType::i32},
                            VectorParam{3, ScalarType::i32},
                            VectorParam{4, ScalarType::i32},
                            VectorParam{2, ScalarType::u32},
                            VectorParam{3, ScalarType::u32},
                            VectorParam{4, ScalarType::u32},
                        });

DAWN_INSTANTIATE_TEST_P(ComputeBufferReadWriteTest_Matrix,
                        {D3D12Backend(), MetalBackend(), OpenGLBackend(), OpenGLESBackend(),
                         VulkanBackend()},
                        {SrcBufferType::UniformBuffer, SrcBufferType::StorageBuffer},
                        {
                            MatrixParam{2, 2, ScalarType::f32},
                            MatrixParam{2, 3, ScalarType::f32},
                            MatrixParam{2, 4, ScalarType::f32},
                            MatrixParam{3, 2, ScalarType::f32},
                            MatrixParam{3, 3, ScalarType::f32},
                            MatrixParam{3, 4, ScalarType::f32},
                            MatrixParam{4, 2, ScalarType::f32},
                            MatrixParam{4, 3, ScalarType::f32},
                            MatrixParam{4, 4, ScalarType::f32},
                        });
