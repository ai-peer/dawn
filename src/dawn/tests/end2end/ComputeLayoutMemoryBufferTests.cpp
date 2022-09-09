// Copyright 2021 The Dawn Authors
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

#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <vector>

#include "dawn/common/Math.h"
#include "dawn/tests/DawnTest.h"
#include "dawn/utils/WGPUHelpers.h"

namespace {

// Helper for replacing all occurrences of substr in str with replacement
std::string ReplaceAll(std::string str, const std::string& substr, const std::string& replacement) {
    size_t pos = 0;
    while ((pos = str.find(substr, pos)) != std::string::npos) {
        str.replace(pos, substr.length(), replacement);
        pos += replacement.length();
    }
    return str;
}

// StorageClass is an enumerator of storage classes used by ComputeLayoutMemoryBufferTests.Fields
enum class StorageClass {
    Uniform,
    Storage,
};

std::ostream& operator<<(std::ostream& o, StorageClass storageClass) {
    switch (storageClass) {
        case StorageClass::Uniform:
            o << "uniform";
            break;
        case StorageClass::Storage:
            o << "storage";
            break;
    }
    return o;
}

// Host-sharable scalar types
enum class ScalarType {
    f32,
    i32,
    u32,
    f16,
};

std::string ScalarTypeName(ScalarType scalarType) {
    switch (scalarType) {
        case ScalarType::f32:
            return "f32";
        case ScalarType::i32:
            return "i32";
        case ScalarType::u32:
            return "u32";
        case ScalarType::f16:
            return "f16";
    }
    return "<unknown scalar type>";
}

size_t ScalarTypeSize(ScalarType scalarType) {
    switch (scalarType) {
        case ScalarType::f32:
        case ScalarType::i32:
        case ScalarType::u32:
            return 4;
        case ScalarType::f16:
            return 2;
    }
    ASSERT(false);
    return 0;
}

class MemoryDataBuilder {
  public:
    // There are three types of operation that operate on a memory buffer `buf`:
    //   1. Align to a specific alignment `alignment`, which will ensure
    //      `buf.size() % alignment == 0` by adding padding bytes into the buffer
    //      if necessary;
    //   2. Add specific `size` bytes of data bytes into buffer;
    //   3. Add specific `size` bytes of padding bytes into buffer;
    //   4. Fill all `size` given (fixed) bytes into the memory buffer.
    // Note that data bytes and padding bytes are generated seperatedly and design to
    // be distinguishable, i.e. data bytes have MSB set to 0 while padding bytes 1.
    enum class OperationType {
        Align,
        Data,
        Padding,
        FillingFixed,
    };
    struct Operation {
        OperationType mType;
        // mOperand is `alignment` for Align operation, and `size` for Data and FillingFixed.
        size_t mOperand;
        // The data that will be filled into buffer if the segment type is FillingFixed. Otherwise
        // for Padding and Data segment, the filling bytes are byte-wise generated based on xor
        // keys.
        std::vector<uint8_t> mFixedFillingData;
    };

    std::vector<Operation> mOperations;

    MemoryDataBuilder& AddFixedU32(uint32_t u32) {
        std::vector<uint8_t> bytes;
        bytes.emplace_back((u32 >> 0) & 0xff);
        bytes.emplace_back((u32 >> 8) & 0xff);
        bytes.emplace_back((u32 >> 16) & 0xff);
        bytes.emplace_back((u32 >> 24) & 0xff);
        return AddFixedBytes(bytes);
    }

    MemoryDataBuilder& AddFixedBytes(std::vector<uint8_t>& bytes) {
        mOperations.push_back({OperationType::FillingFixed, bytes.size(), bytes});
        return *this;
    }

    MemoryDataBuilder& AlignTo(uint32_t alignment) {
        mOperations.push_back({OperationType::Align, alignment, {}});
        return *this;
    }

    MemoryDataBuilder& AddData(size_t size) {
        mOperations.push_back({OperationType::Data, size, {}});
        return *this;
    }

    MemoryDataBuilder& AddPadding(size_t size) {
        mOperations.push_back({OperationType::Padding, size, {}});
        return *this;
    }

    MemoryDataBuilder& AddSubBuilder(MemoryDataBuilder builder) {
        mOperations.insert(mOperations.end(), builder.mOperations.begin(),
                           builder.mOperations.end());
        return *this;
    }

    // dataXorKey and paddingXorKey controls the generated data and padding bytes seperatedly, make
    // it possible to, for example, generate two buffers that have different data bytes but
    // identical padding bytes, thus can be used as initializer and expectation bytes of the copy
    // destination buffer, expecting data bytes are changed while padding bytes are left unchanged.
    void ApplyOperationsToBuffer(std::vector<uint8_t>& buffer,
                                 uint8_t dataXorKey = 0u,
                                 uint8_t paddingXorKey = 0u) {
        uint8_t dataByte = 0x0u;
        uint8_t paddingByte = 0x2u;
        // Get a data byte with MSB set to 0.
        auto NextDataByte = [&]() {
            dataByte += 0x11u;
            return static_cast<uint8_t>((dataByte ^ dataXorKey) & 0x7fu);
        };
        // Get a padding byte with MSB set to 1, distinguished from data bytes.
        auto NextPaddingByte = [&]() {
            paddingByte += 0x13u;
            return static_cast<uint8_t>((paddingByte ^ paddingXorKey) | 0x80u);
        };
        for (auto& operation : mOperations) {
            if (operation.mType == OperationType::FillingFixed) {
                ASSERT(operation.mOperand == operation.mFixedFillingData.size());
                buffer.insert(buffer.end(), operation.mFixedFillingData.begin(),
                              operation.mFixedFillingData.end());
            } else if (operation.mType == OperationType::Align) {
                size_t targetSize = Align(buffer.size(), operation.mOperand);
                size_t paddingSize = targetSize - buffer.size();
                for (size_t i = 0; i < paddingSize; i++) {
                    buffer.push_back(NextPaddingByte());
                }
            } else {
                for (size_t i = 0; i < operation.mOperand; i++) {
                    buffer.push_back((operation.mType == OperationType::Data) ? NextDataByte()
                                                                              : NextPaddingByte());
                }
            }
        }
    }
};

// DataMatcherCallback is the callback function by DataMatcher.
// It is called for each contiguous sequence of bytes that should be checked
// for equality.
// offset and size are in units of bytes.
using DataMatcherCallback = std::function<void(uint32_t offset, uint32_t size)>;

struct Field;

// DataMatcher is a function pointer to a data matching function.
// size is the total number of bytes being considered for matching.
// The callback may be called once or multiple times, and may only consider
// part of the interval [0, size)
using DataMatcher = void (*)(const Field& field, DataMatcherCallback);

void FullDataMatcher(const Field& field, DataMatcherCallback callback);
void StridedDataMatcher(const Field& field, DataMatcherCallback callback);

// Field describe a type that has no padding byte between any two data bytes, e.g. `i32`,
// `vec2<f32>`, `mat4x4<f32>` or `array<f32, 5>`, or have a fixed data stride, e.g. `mat3x3<f32>`
// or `array<vec3<f32>, 4>`. `@size` and `@align` attributes, when used as a struct member, can also
// described by this struct.
struct Field {
    std::string name;  // Friendly name of the type of the field
    size_t align;      // Natural alignment of the type in bytes
    size_t size;       // Natural size of the type in bytes

    bool hasAlignAttribute = false;
    bool hasSizeAttribute = false;
    size_t paddedSize = 0;           // Decorated (extended) size of the type in bytes
    bool storageBufferOnly = false;  // This type doesn't meet the layout constraints for uniform
                                     // buffer and thus should only be used for storage buffer tests

    bool isStrided = false;
    size_t strideDataBytes = 0;
    size_t stridePaddingBytes = 0;

    DataMatcher matcher = &FullDataMatcher;  // The matching method

    // Sets the paddedSize to value.
    // Returns this Field so calls can be chained.
    Field& SizeAttribute(size_t value) {
        ASSERT(value >= size);
        hasSizeAttribute = true;
        paddedSize = value;
        return *this;
    }

    size_t GetPaddedSize() const { return hasSizeAttribute ? paddedSize : size; }

    // Sets the align to value.
    // Returns this Field so calls can be chained.
    Field& AlignAttribute(size_t value) {
        ASSERT(value >= align);
        ASSERT(IsPowerOfTwo(value));
        align = value;
        hasAlignAttribute = true;
        return *this;
    }

    // Sets the matcher to a StridedDataMatcher, and record given strideDataBytes and
    // stridePaddingBytes. Returns this Field so calls can be chained.
    Field& Strided(size_t bytesData, size_t bytesPadding) {
        isStrided = true;
        strideDataBytes = bytesData;
        stridePaddingBytes = bytesPadding;
        matcher = &StridedDataMatcher;
        return *this;
    }

    // Marks that this should only be used for storage buffer tests.
    // Returns this Field so calls can be chained.
    Field& StorageBufferOnly() {
        storageBufferOnly = true;
        return *this;
    }

    // Get a MemoryDataBuilder that do alignment, place data bytes and padding bytes, according to
    // field's alignment, size, padding, and stride information. This MemoryDataBuilder can be used
    // by other MemoryDataBuilder as needed.
    MemoryDataBuilder GetDataBuilder() const {
        MemoryDataBuilder builder;
        builder.AlignTo(align);
        if (isStrided) {
            // Check that stride pattern cover the whole data part, i.e. the data part contains N x
            // whole data bytes and N or (N-1) x whole padding bytes.
            ASSERT((size % (strideDataBytes + stridePaddingBytes) == 0) ||
                   ((size + stridePaddingBytes) % (strideDataBytes + stridePaddingBytes) == 0));
            size_t offset = 0;
            while (offset < size) {
                builder.AddData(strideDataBytes);
                offset += strideDataBytes;
                if (offset < size) {
                    builder.AddPadding(stridePaddingBytes);
                    offset += stridePaddingBytes;
                }
            }
        } else {
            builder.AddData(size);
        }
        if (hasSizeAttribute) {
            builder.AddPadding(paddedSize - size);
        }
        return builder;
    }

    static Field Scalar(ScalarType type) {
        return Field{ScalarTypeName(type), ScalarTypeSize(type), ScalarTypeSize(type)};
    }

    static Field Vector(uint32_t n, ScalarType type) {
        ASSERT(2 <= n && n <= 4);
        size_t elementSize = ScalarTypeSize(type);
        size_t vectorSize = n * elementSize;
        size_t vectorAlignment = (n == 3 ? 4 : n) * elementSize;
        return Field{"vec" + std::to_string(n) + "<" + ScalarTypeName(type) + ">", vectorAlignment,
                     vectorSize};
    }

    static Field Matrix(uint32_t col, uint32_t row, ScalarType type) {
        ASSERT(2 <= col && col <= 4);
        ASSERT(2 <= row && row <= 4);
        ASSERT(type == ScalarType::f32 || type == ScalarType::f16);
        size_t elementSize = ScalarTypeSize(type);
        size_t colVectorSize = row * elementSize;
        size_t colVectorAlignment = (row == 3 ? 4 : row) * elementSize;
        Field field = Field{"mat" + std::to_string(col) + "x" + std::to_string(row) + "<" +
                                ScalarTypeName(type) + ">",
                            colVectorAlignment, col * colVectorAlignment};
        if (colVectorSize != colVectorAlignment) {
            field.Strided(colVectorSize, colVectorAlignment - colVectorSize);
        }
        return field;
    }
};

std::ostream& operator<<(std::ostream& o, Field field) {
    o << "@align(" << field.align << ") @size("
      << (field.paddedSize > 0 ? field.paddedSize : field.size) << ") " << field.name;
    return o;
}

// FullDataMatcher is a DataMatcher that calls callback with the interval
// [0, size)
void FullDataMatcher(const Field& field, DataMatcherCallback callback) {
    callback(0, field.size);
}

// StridedDataMatcher is a DataMatcher that calls callback with the strided
// intervals of length Field.strideDataBytes, skipping Field.stridePaddingBytes.
// For example: StridedDataMatcher(field, callback) with field.size = 18, field.strideDataBytes = 2,
// and field.stridePaddingBytes = 4 will call callback with the intervals: [0, 2), [6, 8), [12, 14)
void StridedDataMatcher(const Field& field, DataMatcherCallback callback) {
    size_t bytesToMatch = field.strideDataBytes;
    size_t bytesToSkip = field.stridePaddingBytes;
    size_t size = field.size;
    size_t offset = 0;
    while (offset < size) {
        callback(offset, bytesToMatch);
        offset += bytesToMatch + bytesToSkip;
    }
}

// Create a compute pipeline with all buffer in bufferList binded in order starting from slot 0, and
// run the given shader.
void RunComputeShaderWithBuffers(const wgpu::Device& device,
                                 const wgpu::Queue& queue,
                                 const std::string& shader,
                                 std::initializer_list<wgpu::Buffer> bufferList) {
    // Set up shader and pipeline
    auto module = utils::CreateShaderModule(device, shader.c_str());

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.compute.module = module;
    csDesc.compute.entryPoint = "main";

    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

    // Set up bind group and issue dispatch
    std::vector<wgpu::BindGroupEntry> entries;
    uint32_t bufferSlot = 0;
    for (const wgpu::Buffer& buffer : bufferList) {
        entries.push_back(utils::BindingInitializationHelper{bufferSlot++, buffer}.GetAsBinding());
    }

    wgpu::BindGroupDescriptor descriptor;
    descriptor.layout = pipeline.GetBindGroupLayout(0);
    descriptor.entryCount = static_cast<uint32_t>(entries.size());
    descriptor.entries = entries.data();

    wgpu::BindGroup bindGroup = device.CreateBindGroup(&descriptor);

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
}

DAWN_TEST_PARAM_STRUCT(ComputeLayoutMemoryBufferTestParams, StorageClass, Field);

class ComputeLayoutMemoryBufferTests
    : public DawnTestWithParams<ComputeLayoutMemoryBufferTestParams> {
    void SetUp() override { DawnTestBase::SetUp(); }
};

// Align returns the WGSL decoration for an explicit structure field alignment
std::string AlignDeco(uint32_t value) {
    return "@align(" + std::to_string(value) + ") ";
}

// Test different types used as a struct member
TEST_P(ComputeLayoutMemoryBufferTests, StructMember) {
    // Sentinel value markers codes used to check that the start and end of
    // structures are correctly aligned. Each of these codes are distinct and
    // are not likely to be confused with data.
    constexpr uint32_t kDataHeaderCode = 0xa0b0c0a0u;
    constexpr uint32_t kDataFooterCode = 0x40302010u;
    constexpr uint32_t kInputHeaderCode = 0x91827364u;
    constexpr uint32_t kInputFooterCode = 0x19283764u;

    // Status codes returned by the shader.
    constexpr uint32_t kStatusBadInputHeader = 100u;
    constexpr uint32_t kStatusBadInputFooter = 101u;
    constexpr uint32_t kStatusBadDataHeader = 102u;
    constexpr uint32_t kStatusBadDataFooter = 103u;
    constexpr uint32_t kStatusOk = 200u;

    const Field& field = GetParam().mField;

    const bool isUniform = GetParam().mStorageClass == StorageClass::Uniform;

    std::string shader = R"(
struct Data {
    header : u32,
    @align({field_align}) @size({field_size}) field : {field_type},
    footer : u32,
}

struct Input {
    header : u32,
    {data_align}data : Data,
    {footer_align}footer : u32,
}

struct Output {
    data : {field_type}
}

struct Status {
    code : u32
}

@group(0) @binding(0) var<{input_qualifiers}> input : Input;
@group(0) @binding(1) var<storage, read_write> output : Output;
@group(0) @binding(2) var<storage, read_write> status : Status;

@compute @workgroup_size(1,1,1)
fn main() {
    if (input.header != {input_header_code}u) {
        status.code = {status_bad_input_header}u;
    } else if (input.footer != {input_footer_code}u) {
        status.code = {status_bad_input_footer}u;
    } else if (input.data.header != {data_header_code}u) {
        status.code = {status_bad_data_header}u;
    } else if (input.data.footer != {data_footer_code}u) {
        status.code = {status_bad_data_footer}u;
    } else {
        status.code = {status_ok}u;
        output.data = input.data.field;
    }
})";

    // https://www.w3.org/TR/WGSL/#alignment-and-size
    // Structure size: roundUp(AlignOf(S), OffsetOf(S, L) + SizeOf(S, L))
    // https://www.w3.org/TR/WGSL/#storage-class-constraints
    // RequiredAlignOf(S, uniform): roundUp(16, max(AlignOf(T0), ..., AlignOf(TN)))
    uint32_t dataAlign = isUniform ? std::max(size_t(16u), field.align) : field.align;

    // https://www.w3.org/TR/WGSL/#structure-layout-rules
    // Note: When underlying the target is a Vulkan device, we assume the device does not support
    // the scalarBlockLayout feature. Therefore, a data value must not be placed in the padding at
    // the end of a structure or matrix, nor in the padding at the last element of an array.
    uint32_t footerAlign = isUniform ? 16 : 4;

    shader = ReplaceAll(shader, "{data_align}", isUniform ? AlignDeco(dataAlign) : "");
    shader = ReplaceAll(shader, "{field_align}", std::to_string(field.align));
    shader = ReplaceAll(shader, "{footer_align}", isUniform ? AlignDeco(footerAlign) : "");
    shader = ReplaceAll(shader, "{field_size}",
                        std::to_string(field.paddedSize > 0 ? field.paddedSize : field.size));
    shader = ReplaceAll(shader, "{field_type}", field.name);
    shader = ReplaceAll(shader, "{input_header_code}", std::to_string(kInputHeaderCode));
    shader = ReplaceAll(shader, "{input_footer_code}", std::to_string(kInputFooterCode));
    shader = ReplaceAll(shader, "{data_header_code}", std::to_string(kDataHeaderCode));
    shader = ReplaceAll(shader, "{data_footer_code}", std::to_string(kDataFooterCode));
    shader = ReplaceAll(shader, "{status_bad_input_header}", std::to_string(kStatusBadInputHeader));
    shader = ReplaceAll(shader, "{status_bad_input_footer}", std::to_string(kStatusBadInputFooter));
    shader = ReplaceAll(shader, "{status_bad_data_header}", std::to_string(kStatusBadDataHeader));
    shader = ReplaceAll(shader, "{status_bad_data_footer}", std::to_string(kStatusBadDataFooter));
    shader = ReplaceAll(shader, "{status_ok}", std::to_string(kStatusOk));
    shader = ReplaceAll(shader, "{input_qualifiers}",
                        isUniform ? "uniform"  //
                                  : "storage, read_write");

    // Build the input and expected data.
    MemoryDataBuilder inputDataBuilder;  // The whole SSBO data
    {
        inputDataBuilder.AddFixedU32(kInputHeaderCode);  // Input.header
        inputDataBuilder.AlignTo(dataAlign);             // Input.data
        {
            inputDataBuilder.AddFixedU32(kDataHeaderCode);  // Input.data.header
            inputDataBuilder.AddSubBuilder(field.GetDataBuilder());
            inputDataBuilder.AddFixedU32(kDataFooterCode);  // Input.data.footer
            inputDataBuilder.AlignTo(field.align);          // Input.data padding
        }
        inputDataBuilder.AlignTo(footerAlign);           // Input.footer @align
        inputDataBuilder.AddFixedU32(kInputFooterCode);  // Input.footer
        inputDataBuilder.AlignTo(256);                   // Input padding
    }
    std::vector<uint8_t> inputData;
    inputDataBuilder.ApplyOperationsToBuffer(inputData);

    MemoryDataBuilder expectedDataBuilder;  // The expected data to be copied by the shader
    expectedDataBuilder.AddSubBuilder(field.GetDataBuilder());
    std::vector<uint8_t> expectedData;
    std::vector<uint8_t> initData;

    // Initialize the dst buffer with different data and padding bytes.
    expectedDataBuilder.ApplyOperationsToBuffer(initData, 0xffu, 0x88u);
    // Expectation with all data bytes equal to src data, but padding bytes equal to initialize
    // bytes.
    expectedDataBuilder.ApplyOperationsToBuffer(expectedData, 0u, 0x88u);

    // Set up input storage buffer
    wgpu::Buffer inputBuf = utils::CreateBufferFromData(
        device, inputData.data(), inputData.size(),
        wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst |
            (isUniform ? wgpu::BufferUsage::Uniform : wgpu::BufferUsage::Storage));

    // Set up output storage buffer
    wgpu::Buffer outputBuf = utils::CreateBufferFromData(
        device, initData.data(), initData.size(),
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst);

    // Set up status storage buffer
    wgpu::BufferDescriptor statusDesc;
    statusDesc.size = 4u;
    statusDesc.usage =
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer statusBuf = device.CreateBuffer(&statusDesc);

    RunComputeShaderWithBuffers(device, queue, shader, {inputBuf, outputBuf, statusBuf});

    // Check the status
    EXPECT_BUFFER_U32_EQ(kStatusOk, statusBuf, 0) << "status code error" << std::endl
                                                  << "Shader: " << shader;

    // Check the data
    field.matcher(field, [&](uint32_t offset, uint32_t size) {
        EXPECT_BUFFER_U8_RANGE_EQ(expectedData.data() + offset, outputBuf, offset, size)
            << "offset: " << offset;
    });
}

// Test different types that used directly as buffer type
TEST_P(ComputeLayoutMemoryBufferTests, NonStructMember) {
    auto params = GetParam();
    Field& field = params.mField;
    // @size and @align attribute only apply to struct members, skip them
    if (field.hasSizeAttribute | field.hasAlignAttribute) {
        return;
    }

    const bool isUniform = GetParam().mStorageClass == StorageClass::Uniform;

    std::string shader = R"(
@group(0) @binding(0) var<{input_qualifiers}> input : {field_type};
@group(0) @binding(1) var<storage, read_write> output : {field_type};

@compute @workgroup_size(1,1,1)
fn main() {
        output = input;
})";

    shader = ReplaceAll(shader, "{field_type}", field.name);
    shader = ReplaceAll(shader, "{input_qualifiers}",
                        isUniform ? "uniform"  //
                                  : "storage, read_write");

    // Build the input and expected data.
    MemoryDataBuilder dataBuilder;
    dataBuilder.AddSubBuilder(field.GetDataBuilder());

    std::vector<uint8_t> inputData;
    std::vector<uint8_t> initData;
    std::vector<uint8_t> expectedData;

    dataBuilder.ApplyOperationsToBuffer(inputData, 0x00u, 0x00u);
    // Initialize the dst buffer with different data and padding bytes.
    dataBuilder.ApplyOperationsToBuffer(initData, 0xffu, 0x77u);
    // Expectation with all data bytes equal to src data, but padding bytes equal to initialize
    // bytes.
    dataBuilder.ApplyOperationsToBuffer(expectedData, 0x00u, 0x77u);

    // Set up input storage buffer
    wgpu::Buffer inputBuf = utils::CreateBufferFromData(
        device, inputData.data(), inputData.size(),
        wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst |
            (isUniform ? wgpu::BufferUsage::Uniform : wgpu::BufferUsage::Storage));
    EXPECT_BUFFER_U8_RANGE_EQ(inputData.data(), inputBuf, 0, inputData.size());

    // Set up output storage buffer
    wgpu::Buffer outputBuf = utils::CreateBufferFromData(
        device, initData.data(), initData.size(),
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst);
    EXPECT_BUFFER_U8_RANGE_EQ(initData.data(), outputBuf, 0, initData.size());

    RunComputeShaderWithBuffers(device, queue, shader, {inputBuf, outputBuf});

    // Check the data
    field.matcher(field, [&](uint32_t offset, uint32_t size) {
        EXPECT_BUFFER_U8_RANGE_EQ(expectedData.data() + offset, outputBuf, offset, size)
            << "offset: " << offset;
    });
}

auto GenerateParams() {
    auto params = MakeParamGenerator<ComputeLayoutMemoryBufferTestParams>(
        {
            D3D12Backend(),
            MetalBackend(),
            VulkanBackend(),
            OpenGLBackend(),
            OpenGLESBackend(),
        },
        {StorageClass::Storage, StorageClass::Uniform},
        {
            // See https://www.w3.org/TR/WGSL/#alignment-and-size
            // Scalar types with no custom alignment or size
            Field::Scalar(ScalarType::f32),
            Field::Scalar(ScalarType::i32),
            Field::Scalar(ScalarType::u32),

            // Scalar types with custom alignment
            Field::Scalar(ScalarType::f32).AlignAttribute(16),
            Field::Scalar(ScalarType::i32).AlignAttribute(16),
            Field::Scalar(ScalarType::u32).AlignAttribute(16),

            // Scalar types with custom size
            Field::Scalar(ScalarType::f32).SizeAttribute(24),
            Field::Scalar(ScalarType::i32).SizeAttribute(24),
            Field::Scalar(ScalarType::u32).SizeAttribute(24),

            // Vector types with no custom alignment or size
            Field::Vector(2, ScalarType::f32),
            Field::Vector(3, ScalarType::f32),
            Field::Vector(4, ScalarType::f32),
            Field::Vector(2, ScalarType::i32),
            Field::Vector(3, ScalarType::i32),
            Field::Vector(4, ScalarType::i32),
            Field::Vector(2, ScalarType::u32),
            Field::Vector(3, ScalarType::u32),
            Field::Vector(4, ScalarType::u32),

            // Vector types with custom alignment
            Field::Vector(2, ScalarType::f32).AlignAttribute(32),
            Field::Vector(3, ScalarType::f32).AlignAttribute(32),
            Field::Vector(4, ScalarType::f32).AlignAttribute(32),
            Field::Vector(2, ScalarType::i32).AlignAttribute(32),
            Field::Vector(3, ScalarType::i32).AlignAttribute(32),
            Field::Vector(4, ScalarType::i32).AlignAttribute(32),
            Field::Vector(2, ScalarType::u32).AlignAttribute(32),
            Field::Vector(3, ScalarType::u32).AlignAttribute(32),
            Field::Vector(4, ScalarType::u32).AlignAttribute(32),

            // Vector types with custom size
            Field::Vector(2, ScalarType::f32).SizeAttribute(24),
            Field::Vector(3, ScalarType::f32).SizeAttribute(24),
            Field::Vector(4, ScalarType::f32).SizeAttribute(24),
            Field::Vector(2, ScalarType::i32).SizeAttribute(24),
            Field::Vector(3, ScalarType::i32).SizeAttribute(24),
            Field::Vector(4, ScalarType::i32).SizeAttribute(24),
            Field::Vector(2, ScalarType::u32).SizeAttribute(24),
            Field::Vector(3, ScalarType::u32).SizeAttribute(24),
            Field::Vector(4, ScalarType::u32).SizeAttribute(24),

            // Matrix types with no custom alignment or size
            Field::Matrix(2, 2, ScalarType::f32),
            Field::Matrix(3, 2, ScalarType::f32),
            Field::Matrix(4, 2, ScalarType::f32),
            Field::Matrix(2, 3, ScalarType::f32),
            Field::Matrix(3, 3, ScalarType::f32),
            Field::Matrix(4, 3, ScalarType::f32),
            Field::Matrix(2, 4, ScalarType::f32),
            Field::Matrix(3, 4, ScalarType::f32),
            Field::Matrix(4, 4, ScalarType::f32),

            // Matrix types with custom alignment
            Field::Matrix(2, 2, ScalarType::f32).AlignAttribute(32),
            Field::Matrix(3, 2, ScalarType::f32).AlignAttribute(32),
            Field::Matrix(4, 2, ScalarType::f32).AlignAttribute(32),
            Field::Matrix(2, 3, ScalarType::f32).AlignAttribute(32),
            Field::Matrix(3, 3, ScalarType::f32).AlignAttribute(32),
            Field::Matrix(4, 3, ScalarType::f32).AlignAttribute(32),
            Field::Matrix(2, 4, ScalarType::f32).AlignAttribute(32),
            Field::Matrix(3, 4, ScalarType::f32).AlignAttribute(32),
            Field::Matrix(4, 4, ScalarType::f32).AlignAttribute(32),

            // Matrix types with custom size
            Field::Matrix(2, 2, ScalarType::f32).SizeAttribute(128),
            Field::Matrix(3, 2, ScalarType::f32).SizeAttribute(128),
            Field::Matrix(4, 2, ScalarType::f32).SizeAttribute(128),
            Field::Matrix(2, 3, ScalarType::f32).SizeAttribute(128),
            Field::Matrix(3, 3, ScalarType::f32).SizeAttribute(128),
            Field::Matrix(4, 3, ScalarType::f32).SizeAttribute(128),
            Field::Matrix(2, 4, ScalarType::f32).SizeAttribute(128),
            Field::Matrix(3, 4, ScalarType::f32).SizeAttribute(128),
            Field::Matrix(4, 4, ScalarType::f32).SizeAttribute(128),

            // Array types with no custom alignment or size.
            // Note: The use of StorageBufferOnly() is due to UBOs requiring 16 byte alignment
            // of array elements. See https://www.w3.org/TR/WGSL/#storage-class-constraints
            Field{"array<u32, 1>", /* align */ 4, /* size */ 4}.StorageBufferOnly(),
            Field{"array<u32, 2>", /* align */ 4, /* size */ 8}.StorageBufferOnly(),
            Field{"array<u32, 3>", /* align */ 4, /* size */ 12}.StorageBufferOnly(),
            Field{"array<u32, 4>", /* align */ 4, /* size */ 16}.StorageBufferOnly(),
            Field{"array<vec2<u32>, 1>", /* align */ 8, /* size */ 8}.StorageBufferOnly(),
            Field{"array<vec2<u32>, 2>", /* align */ 8, /* size */ 16}.StorageBufferOnly(),
            Field{"array<vec2<u32>, 3>", /* align */ 8, /* size */ 24}.StorageBufferOnly(),
            Field{"array<vec2<u32>, 4>", /* align */ 8, /* size */ 32}.StorageBufferOnly(),
            Field{"array<vec3<u32>, 1>", /* align */ 16, /* size */ 16}.Strided(12, 4),
            Field{"array<vec3<u32>, 2>", /* align */ 16, /* size */ 32}.Strided(12, 4),
            Field{"array<vec3<u32>, 3>", /* align */ 16, /* size */ 48}.Strided(12, 4),
            Field{"array<vec3<u32>, 4>", /* align */ 16, /* size */ 64}.Strided(12, 4),
            Field{"array<vec4<u32>, 1>", /* align */ 16, /* size */ 16},
            Field{"array<vec4<u32>, 2>", /* align */ 16, /* size */ 32},
            Field{"array<vec4<u32>, 3>", /* align */ 16, /* size */ 48},
            Field{"array<vec4<u32>, 4>", /* align */ 16, /* size */ 64},

            // Array types with custom alignment
            Field{"array<u32, 1>", /* align */ 4, /* size */ 4}
                .AlignAttribute(32)
                .StorageBufferOnly(),
            Field{"array<u32, 2>", /* align */ 4, /* size */ 8}
                .AlignAttribute(32)
                .StorageBufferOnly(),
            Field{"array<u32, 3>", /* align */ 4, /* size */ 12}
                .AlignAttribute(32)
                .StorageBufferOnly(),
            Field{"array<u32, 4>", /* align */ 4, /* size */ 16}
                .AlignAttribute(32)
                .StorageBufferOnly(),
            Field{"array<vec2<u32>, 1>", /* align */ 8, /* size */ 8}
                .AlignAttribute(32)
                .StorageBufferOnly(),
            Field{"array<vec2<u32>, 2>", /* align */ 8, /* size */ 16}
                .AlignAttribute(32)
                .StorageBufferOnly(),
            Field{"array<vec2<u32>, 3>", /* align */ 8, /* size */ 24}
                .AlignAttribute(32)
                .StorageBufferOnly(),
            Field{"array<vec2<u32>, 4>", /* align */ 8, /* size */ 32}
                .AlignAttribute(32)
                .StorageBufferOnly(),
            Field{"array<vec3<u32>, 1>", /* align */ 16, /* size */ 16}.AlignAttribute(32).Strided(
                12, 4),
            Field{"array<vec3<u32>, 2>", /* align */ 16, /* size */ 32}.AlignAttribute(32).Strided(
                12, 4),
            Field{"array<vec3<u32>, 3>", /* align */ 16, /* size */ 48}.AlignAttribute(32).Strided(
                12, 4),
            Field{"array<vec3<u32>, 4>", /* align */ 16, /* size */ 64}.AlignAttribute(32).Strided(
                12, 4),
            Field{"array<vec4<u32>, 1>", /* align */ 16, /* size */ 16}.AlignAttribute(32),
            Field{"array<vec4<u32>, 2>", /* align */ 16, /* size */ 32}.AlignAttribute(32),
            Field{"array<vec4<u32>, 3>", /* align */ 16, /* size */ 48}.AlignAttribute(32),
            Field{"array<vec4<u32>, 4>", /* align */ 16, /* size */ 64}.AlignAttribute(32),

            // Array types with custom size
            Field{"array<u32, 1>", /* align */ 4, /* size */ 4}
                .SizeAttribute(128)
                .StorageBufferOnly(),
            Field{"array<u32, 2>", /* align */ 4, /* size */ 8}
                .SizeAttribute(128)
                .StorageBufferOnly(),
            Field{"array<u32, 3>", /* align */ 4, /* size */ 12}
                .SizeAttribute(128)
                .StorageBufferOnly(),
            Field{"array<u32, 4>", /* align */ 4, /* size */ 16}
                .SizeAttribute(128)
                .StorageBufferOnly(),
            Field{"array<vec3<u32>, 4>", /* align */ 16, /* size */ 64}.SizeAttribute(128).Strided(
                12, 4),
        });

    std::vector<ComputeLayoutMemoryBufferTestParams> filtered;
    for (auto param : params) {
        if (param.mStorageClass != StorageClass::Storage && param.mField.storageBufferOnly) {
            continue;
        }
        filtered.emplace_back(param);
    }
    return filtered;
}

INSTANTIATE_TEST_SUITE_P(,
                         ComputeLayoutMemoryBufferTests,
                         ::testing::ValuesIn(GenerateParams()),
                         DawnTestBase::PrintToStringParamName("ComputeLayoutMemoryBufferTests"));
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ComputeLayoutMemoryBufferTests);

}  // namespace
