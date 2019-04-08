// Copyright 2019 The Dawn Authors
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

#include "tests/DawnTest.h"

#include "common/Assert.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/DawnHelpers.h"
#include "utils/Math.h"

constexpr uint32_t kRTSize = 400;
constexpr uint32_t kVertexNum = 3;

class VertexFormatTest : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);
    }

    utils::BasicRenderPass renderPass;

    bool IsNormalizedFormat(dawn::VertexFormat format) {
        switch (format) {
            case dawn::VertexFormat::UChar2Norm:
            case dawn::VertexFormat::UChar4Norm:
            case dawn::VertexFormat::Char2Norm:
            case dawn::VertexFormat::Char4Norm:
            case dawn::VertexFormat::UShort2Norm:
            case dawn::VertexFormat::UShort4Norm:
            case dawn::VertexFormat::Short2Norm:
            case dawn::VertexFormat::Short4Norm:
                return true;
            default:
                return false;
        }
    }

    bool IsUnsignedFormat(dawn::VertexFormat format) {
        switch (format) {
            case dawn::VertexFormat::UInt:
            case dawn::VertexFormat::UChar2:
            case dawn::VertexFormat::UChar4:
            case dawn::VertexFormat::UShort2:
            case dawn::VertexFormat::UShort4:
            case dawn::VertexFormat::UInt2:
            case dawn::VertexFormat::UInt3:
            case dawn::VertexFormat::UInt4:
            case dawn::VertexFormat::UChar2Norm:
            case dawn::VertexFormat::UChar4Norm:
            case dawn::VertexFormat::UShort2Norm:
            case dawn::VertexFormat::UShort4Norm:
                return true;
            default:
                return false;
        }
    }

    bool IsFloatFormat(dawn::VertexFormat format) {
        switch (format) {
            case dawn::VertexFormat::Half2:
            case dawn::VertexFormat::Half4:
            case dawn::VertexFormat::Float:
            case dawn::VertexFormat::Float2:
            case dawn::VertexFormat::Float3:
            case dawn::VertexFormat::Float4:
                return true;
            default:
                return false;
        }
    }

    uint32_t BytesPerComponents(dawn::VertexFormat format) {
        switch (format) {
            case dawn::VertexFormat::Char2:
            case dawn::VertexFormat::Char4:
            case dawn::VertexFormat::UChar2:
            case dawn::VertexFormat::UChar4:
            case dawn::VertexFormat::UChar2Norm:
            case dawn::VertexFormat::UChar4Norm:
            case dawn::VertexFormat::Char2Norm:
            case dawn::VertexFormat::Char4Norm:
                return 1;
            case dawn::VertexFormat::UShort2:
            case dawn::VertexFormat::UShort4:
            case dawn::VertexFormat::Short2:
            case dawn::VertexFormat::Short4:
            case dawn::VertexFormat::UShort2Norm:
            case dawn::VertexFormat::UShort4Norm:
            case dawn::VertexFormat::Short2Norm:
            case dawn::VertexFormat::Short4Norm:
            case dawn::VertexFormat::Half2:
            case dawn::VertexFormat::Half4:
                return 2;
            case dawn::VertexFormat::UInt:
            case dawn::VertexFormat::Int:
            case dawn::VertexFormat::Float:
            case dawn::VertexFormat::UInt2:
            case dawn::VertexFormat::UInt3:
            case dawn::VertexFormat::UInt4:
            case dawn::VertexFormat::Int2:
            case dawn::VertexFormat::Int3:
            case dawn::VertexFormat::Int4:
            case dawn::VertexFormat::Float2:
            case dawn::VertexFormat::Float3:
            case dawn::VertexFormat::Float4:
                return 4;
            default:
                DAWN_UNREACHABLE();
        }
    }

    uint32_t ComponentsNum(dawn::VertexFormat format) {
        switch (format) {
            case dawn::VertexFormat::UInt:
            case dawn::VertexFormat::Int:
            case dawn::VertexFormat::Float:
                return 1;
            case dawn::VertexFormat::UChar2:
            case dawn::VertexFormat::UShort2:
            case dawn::VertexFormat::UInt2:
            case dawn::VertexFormat::Char2:
            case dawn::VertexFormat::Short2:
            case dawn::VertexFormat::Int2:
            case dawn::VertexFormat::UChar2Norm:
            case dawn::VertexFormat::Char2Norm:
            case dawn::VertexFormat::UShort2Norm:
            case dawn::VertexFormat::Short2Norm:
            case dawn::VertexFormat::Half2:
            case dawn::VertexFormat::Float2:
                return 2;
            case dawn::VertexFormat::Int3:
            case dawn::VertexFormat::UInt3:
            case dawn::VertexFormat::Float3:
                return 3;
            case dawn::VertexFormat::UChar4:
            case dawn::VertexFormat::UShort4:
            case dawn::VertexFormat::UInt4:
            case dawn::VertexFormat::Char4:
            case dawn::VertexFormat::Short4:
            case dawn::VertexFormat::Int4:
            case dawn::VertexFormat::UChar4Norm:
            case dawn::VertexFormat::Char4Norm:
            case dawn::VertexFormat::UShort4Norm:
            case dawn::VertexFormat::Short4Norm:
            case dawn::VertexFormat::Half4:
            case dawn::VertexFormat::Float4:
                return 4;
        }
    }

    std::string AssignmentInVS(bool isFloat,
                               bool isNormalized,
                               bool isUnsigned,
                               uint32_t componentsNum) {
        if (componentsNum == 1) {
            if (isFloat) {
                return "float";
            } else if (isUnsigned) {
                return "uint";
            } else {
                return "int";
            }
        } else {
            if (isFloat || isNormalized) {
                return "vec";
            } else if (isUnsigned) {
                return "uvec";
            } else {
                return "ivec";
            }
        }
    }

    // The length of vertexData is fixed to 3, it aligns to triangle vertex number
    template <typename T>
    dawn::RenderPipeline MakeTestPipeline(dawn::VertexFormat format, T* expectedData) {
        bool isFloat = IsFloatFormat(format);
        bool isNormalized = IsNormalizedFormat(format);
        bool isUnsigned = IsUnsignedFormat(format);

        uint32_t bytesPerComponents = BytesPerComponents(format);
        uint32_t componentsNum = ComponentsNum(format);

        std::string assignment = AssignmentInVS(isFloat, isNormalized, isUnsigned, componentsNum);
        std::ostringstream vs;
        vs << "#version 450\n";

        vs << "layout(location = 0) in " << assignment
           << (componentsNum == 1 ? "" : std::to_string(componentsNum)) << " test;\n";
        vs << "layout(location = 0) out vec4 color;\n";
        vs << "void main() {\n";

        vs << "    const vec2 pos[3] = vec2[3](vec2(-1.0f, 0.0f), vec2(-1.0f, -1.0f), vec2(0.0f, "
              "-1.0f));\n";
        vs << "    gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);\n";
        vs << "    const " << assignment
           << (componentsNum == 1 ? "" : std::to_string(componentsNum))
           << " expected[3] = " << assignment
           << (componentsNum == 1 ? "" : std::to_string(componentsNum)) << "[3](";
        for (uint32_t i = 0, outCommaPos = kVertexNum - 1; i < kVertexNum; ++i, --outCommaPos) {
            vs << assignment << (componentsNum == 1 ? "" : std::to_string(componentsNum)) << "(";
            for (uint32_t j = 0, innerCommaPos = componentsNum - 1; j < componentsNum;
                 ++j, --innerCommaPos) {
                if (bytesPerComponents == 1 && !isNormalized) {
                    isUnsigned ? vs << static_cast<uint32_t>(expectedData[i * componentsNum + j])
                               : vs << static_cast<int>(expectedData[i * componentsNum + j]);
                } else {
                    vs << expectedData[i * componentsNum + j];
                }
                vs << (innerCommaPos == 0 ? "" : ", ");
            }
            vs << ")" << (outCommaPos == 0 ? "" : ", ");
        }
        vs << ");\n";
        vs << "    float threshold = 1.0 / 64.0;\n";
        vs << "    bool success = true;\n";
        vs << "    success = success";
        if (componentsNum > 1) {
            for (uint32_t component = 0; component < componentsNum; ++component) {
                vs << " && abs(test[" << component << "] - expected[gl_VertexIndex][" << component
                   << "]) < threshold ";
            }
        } else {
            vs << " && abs(test - expected[gl_VertexIndex]) < threshold";
        }
        vs << ";\n";
        vs << "    if (success) {\n";
        vs << "        color = vec4(0.0f, 1.0f, 0.0f, 1.0f);\n";
        vs << "    } else {\n";
        vs << "        color = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n";
        vs << "    }\n";
        vs << "}\n";

        dawn::ShaderModule vsModule =
            utils::CreateShaderModule(device, dawn::ShaderStage::Vertex, vs.str().c_str());

        dawn::ShaderModule fsModule =
            utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, R"(
                #version 450
                layout(location = 0) in vec4 color;
                layout(location = 0) out vec4 fragColor;
                void main() {
                    fragColor = color; 
                })");

        utils::ComboRenderPipelineDescriptor descriptor(device);
        descriptor.cVertexStage.module = vsModule;
        descriptor.cFragmentStage.module = fsModule;
        descriptor.cInputState.numInputs = 1;
        descriptor.cInputState.cInputs[0].stride =
            ComponentsNum(format) * BytesPerComponents(format);
        descriptor.cInputState.numAttributes = 1;
        descriptor.cInputState.cAttributes[0].format = format;
        descriptor.cColorStates[0]->format = renderPass.colorFormat;

        return device.CreateRenderPipeline(&descriptor);
    }
};

TEST_P(VertexFormatTest, UChar2) {
    uint8_t vertexData[8] = {
        std::numeric_limits<uint8_t>::max(),
        0,
        std::numeric_limits<uint8_t>::min(),
        2,
        200,
        201,
        0,
        0  // padding two bytes for buffer copy
    };

    dawn::RenderPipeline pipeline =
        MakeTestPipeline<uint8_t>(dawn::VertexFormat::UChar2, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, UChar4) {
    uint8_t vertexData[12] = {
        std::numeric_limits<uint8_t>::max(),
        0,
        1,
        2,
        std::numeric_limits<uint8_t>::min(),
        2,
        3,
        4,
        200,
        201,
        202,
        203,
    };

    dawn::RenderPipeline pipeline =
        MakeTestPipeline<uint8_t>(dawn::VertexFormat::UChar4, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, Char2) {
    int8_t vertexData[8] = {
        std::numeric_limits<int8_t>::max(),
        0,
        std::numeric_limits<int8_t>::min(),
        -2,
        120,
        -121,
        0,
        0  // padding two bytes for buffer copy
    };

    dawn::RenderPipeline pipeline = MakeTestPipeline<int8_t>(dawn::VertexFormat::Char2, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, Char4) {
    int8_t vertexData[12] = {
        std::numeric_limits<int8_t>::max(),
        0,
        -1,
        2,
        std::numeric_limits<int8_t>::min(),
        -2,
        3,
        4,
        120,
        -121,
        122,
        -123,
    };

    dawn::RenderPipeline pipeline = MakeTestPipeline<int8_t>(dawn::VertexFormat::Char4, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, UChar2Norm) {
    uint8_t vertexData[8] = {
        std::numeric_limits<uint8_t>::max(),
        std::numeric_limits<uint8_t>::min(),
        std::numeric_limits<uint8_t>::max() / 2,
        std::numeric_limits<uint8_t>::min() / 2,
        200,
        201,
        0,
        0  // padding two bytes for buffer copy
    };

    float expectedData[6] = {utils::Normalize(vertexData[0]), utils::Normalize(vertexData[1]),
                             utils::Normalize(vertexData[2]), utils::Normalize(vertexData[3]),
                             utils::Normalize(vertexData[4]), utils::Normalize(vertexData[5])};
    dawn::RenderPipeline pipeline =
        MakeTestPipeline<float>(dawn::VertexFormat::UChar2Norm, expectedData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, UChar4Norm) {
    uint8_t vertexData[12] = {std::numeric_limits<uint8_t>::max(),
                              std::numeric_limits<uint8_t>::min(),
                              0,
                              0,
                              std::numeric_limits<uint8_t>::max() / 2,
                              std::numeric_limits<uint8_t>::min() / 2,
                              0,
                              0,
                              200,
                              201,
                              202,
                              203};

    float expectedData[12] = {
        utils::Normalize(vertexData[0]),  utils::Normalize(vertexData[1]),
        utils::Normalize(vertexData[2]),  utils::Normalize(vertexData[3]),
        utils::Normalize(vertexData[4]),  utils::Normalize(vertexData[5]),
        utils::Normalize(vertexData[6]),  utils::Normalize(vertexData[7]),
        utils::Normalize(vertexData[8]),  utils::Normalize(vertexData[9]),
        utils::Normalize(vertexData[10]), utils::Normalize(vertexData[11]),
    };
    dawn::RenderPipeline pipeline =
        MakeTestPipeline<float>(dawn::VertexFormat::UChar4Norm, expectedData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, Char2Norm) {
    int8_t vertexData[8] = {
        std::numeric_limits<int8_t>::max(),
        std::numeric_limits<int8_t>::min(),
        std::numeric_limits<int8_t>::max() / 2,
        std::numeric_limits<int8_t>::min() / 2,
        120,
        -121,
        0,
        0  // padding two bytes for buffer copy
    };

    float expectedData[6] = {utils::Normalize(vertexData[0]), utils::Normalize(vertexData[1]),
                             utils::Normalize(vertexData[2]), utils::Normalize(vertexData[3]),
                             utils::Normalize(vertexData[4]), utils::Normalize(vertexData[5])};
    dawn::RenderPipeline pipeline =
        MakeTestPipeline<float>(dawn::VertexFormat::Char2Norm, expectedData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, Char4Norm) {
    int8_t vertexData[12] = {std::numeric_limits<int8_t>::max(),
                             std::numeric_limits<int8_t>::min(),
                             0,
                             0,
                             std::numeric_limits<int8_t>::max() / 2,
                             std::numeric_limits<int8_t>::min() / 2,
                             -2,
                             2,
                             120,
                             -120,
                             102,
                             -123};

    float expectedData[12] = {
        utils::Normalize(vertexData[0]),  utils::Normalize(vertexData[1]),
        utils::Normalize(vertexData[2]),  utils::Normalize(vertexData[3]),
        utils::Normalize(vertexData[4]),  utils::Normalize(vertexData[5]),
        utils::Normalize(vertexData[6]),  utils::Normalize(vertexData[7]),
        utils::Normalize(vertexData[8]),  utils::Normalize(vertexData[9]),
        utils::Normalize(vertexData[10]), utils::Normalize(vertexData[11]),
    };
    dawn::RenderPipeline pipeline =
        MakeTestPipeline<float>(dawn::VertexFormat::Char4Norm, expectedData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, UShort2) {
    uint16_t vertexData[6] = {std::numeric_limits<uint16_t>::max(),
                              0,
                              std::numeric_limits<uint16_t>::min(),
                              2,
                              65432,
                              4890};

    dawn::RenderPipeline pipeline =
        MakeTestPipeline<uint16_t>(dawn::VertexFormat::UShort2, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, UShort4) {
    uint16_t vertexData[12] = {
        std::numeric_limits<uint16_t>::max(),
        std::numeric_limits<uint8_t>::max(),
        1,
        2,
        std::numeric_limits<uint16_t>::min(),
        2,
        3,
        4,
        65520,
        65521,
        3435,
        3467,
    };

    dawn::RenderPipeline pipeline =
        MakeTestPipeline<uint16_t>(dawn::VertexFormat::UShort4, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, Short2) {
    int16_t vertexData[6] = {std::numeric_limits<int16_t>::max(),
                             0,
                             std::numeric_limits<int16_t>::min(),
                             -2,
                             3876,
                             -3948};

    dawn::RenderPipeline pipeline =
        MakeTestPipeline<int16_t>(dawn::VertexFormat::Short2, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, Short4) {
    int16_t vertexData[12] = {
        std::numeric_limits<int16_t>::max(),
        0,
        -1,
        2,
        std::numeric_limits<int16_t>::min(),
        -2,
        3,
        4,
        24567,
        -23545,
        4350,
        -2987,
    };

    dawn::RenderPipeline pipeline =
        MakeTestPipeline<int16_t>(dawn::VertexFormat::Short4, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, UShort2Norm) {
    uint16_t vertexData[6] = {std::numeric_limits<uint16_t>::max(),
                              std::numeric_limits<uint16_t>::min(),
                              std::numeric_limits<uint16_t>::max() / 2,
                              std::numeric_limits<uint16_t>::min() / 2,
                              3456,
                              6543};

    float expectedData[6] = {utils::Normalize(vertexData[0]), utils::Normalize(vertexData[1]),
                             utils::Normalize(vertexData[2]), utils::Normalize(vertexData[3]),
                             utils::Normalize(vertexData[4]), utils::Normalize(vertexData[5])};
    dawn::RenderPipeline pipeline =
        MakeTestPipeline<float>(dawn::VertexFormat::UShort2Norm, expectedData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, UShort4Norm) {
    uint16_t vertexData[12] = {std::numeric_limits<uint16_t>::max(),
                               std::numeric_limits<uint16_t>::min(),
                               0,
                               0,
                               std::numeric_limits<uint16_t>::max() / 2,
                               std::numeric_limits<uint16_t>::min() / 2,
                               0,
                               0,
                               2987,
                               3055,
                               2987,
                               2987};

    float expectedData[12] = {
        utils::Normalize(vertexData[0]),  utils::Normalize(vertexData[1]),
        utils::Normalize(vertexData[2]),  utils::Normalize(vertexData[3]),
        utils::Normalize(vertexData[4]),  utils::Normalize(vertexData[5]),
        utils::Normalize(vertexData[6]),  utils::Normalize(vertexData[7]),
        utils::Normalize(vertexData[8]),  utils::Normalize(vertexData[9]),
        utils::Normalize(vertexData[10]), utils::Normalize(vertexData[11]),
    };
    dawn::RenderPipeline pipeline =
        MakeTestPipeline<float>(dawn::VertexFormat::UShort4Norm, expectedData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, Short2Norm) {
    int16_t vertexData[6] = {std::numeric_limits<int16_t>::max(),
                             std::numeric_limits<int16_t>::min(),
                             std::numeric_limits<int16_t>::max() / 2,
                             std::numeric_limits<int16_t>::min() / 2,
                             4987,
                             -6789};

    float expectedData[6] = {utils::Normalize(vertexData[0]), utils::Normalize(vertexData[1]),
                             utils::Normalize(vertexData[2]), utils::Normalize(vertexData[3]),
                             utils::Normalize(vertexData[4]), utils::Normalize(vertexData[5])};
    dawn::RenderPipeline pipeline =
        MakeTestPipeline<float>(dawn::VertexFormat::Short2Norm, expectedData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, Short4Norm) {
    int16_t vertexData[12] = {std::numeric_limits<int16_t>::max(),
                              std::numeric_limits<int16_t>::min(),
                              0,
                              0,
                              std::numeric_limits<int16_t>::max() / 2,
                              std::numeric_limits<int16_t>::min() / 2,
                              -2,
                              2,
                              2890,
                              -29011,
                              20432,
                              -2083};

    float expectedData[12] = {
        utils::Normalize(vertexData[0]),  utils::Normalize(vertexData[1]),
        utils::Normalize(vertexData[2]),  utils::Normalize(vertexData[3]),
        utils::Normalize(vertexData[4]),  utils::Normalize(vertexData[5]),
        utils::Normalize(vertexData[6]),  utils::Normalize(vertexData[7]),
        utils::Normalize(vertexData[8]),  utils::Normalize(vertexData[9]),
        utils::Normalize(vertexData[10]), utils::Normalize(vertexData[11]),
    };
    dawn::RenderPipeline pipeline =
        MakeTestPipeline<float>(dawn::VertexFormat::Short4Norm, expectedData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, Half2) {
    uint16_t vertexData[6] = {
        utils::float32ToFloat16(32.1), utils::float32ToFloat16(-16.8),
        utils::float32ToFloat16(18.2), utils::float32ToFloat16(-24.7),
        utils::float32ToFloat16(12.5), utils::float32ToFloat16(-18.2),
    };

    float expectedData[6] = {32.1f, -16.8f, 18.2f, -24.7f, 12.5f, -18.2f};

    dawn::RenderPipeline pipeline =
        MakeTestPipeline<float>(dawn::VertexFormat::Half2, expectedData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, Half4) {
    uint16_t vertexData[12] = {
        utils::float32ToFloat16(32.1), utils::float32ToFloat16(-16.8),
        utils::float32ToFloat16(18.2), utils::float32ToFloat16(-24.7),
        utils::float32ToFloat16(12.5), utils::float32ToFloat16(-18.2),
        utils::float32ToFloat16(14.8), utils::float32ToFloat16(-12.4),
        utils::float32ToFloat16(22.5), utils::float32ToFloat16(-48.8),
        utils::float32ToFloat16(47.4), utils::float32ToFloat16(-24.8),
    };

    float expectedData[12] = {
        32.1f, -16.8f, 18.2f, -24.7f, 12.5f, -18.2f, 14.8f, -12.4f, 22.5f, -48.8f, 47.4f, -24.8f,
    };

    dawn::RenderPipeline pipeline =
        MakeTestPipeline<float>(dawn::VertexFormat::Half4, expectedData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, Float) {
    float vertexData[3] = {1.0f, -1.0f, 1.0f};
    dawn::RenderPipeline pipeline = MakeTestPipeline<float>(dawn::VertexFormat::Float, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

// Test that the Float4 vertex format is correctly interpreted
TEST_P(VertexFormatTest, Float2) {
    float vertexData[6] = {1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 1.6f};
    dawn::RenderPipeline pipeline = MakeTestPipeline<float>(dawn::VertexFormat::Float2, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

// Test that the Float4 vertex format is correctly interpreted
TEST_P(VertexFormatTest, Float3) {
    float vertexData[9] = {
        1.0f, -1.0f, 0.0f, 1.0f, -5.8f, 99.45f, 23.6f, -81.2f, 55.0f,
    };
    dawn::RenderPipeline pipeline = MakeTestPipeline<float>(dawn::VertexFormat::Float3, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

// Test that the Float4 vertex format is correctly interpreted
TEST_P(VertexFormatTest, Float4) {
    float vertexData[12] = {
        1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.1f, -1.1f, -1.2f,
    };
    dawn::RenderPipeline pipeline = MakeTestPipeline<float>(dawn::VertexFormat::Float4, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, UInt) {
    uint32_t vertexData[3] = {std::numeric_limits<uint32_t>::max(),
                              std::numeric_limits<uint16_t>::max(),
                              std::numeric_limits<uint8_t>::max()};
    dawn::RenderPipeline pipeline =
        MakeTestPipeline<uint32_t>(dawn::VertexFormat::UInt, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

// Test that the Float4 vertex format is correctly interpreted
TEST_P(VertexFormatTest, UInt2) {
    uint32_t vertexData[6] = {std::numeric_limits<uint32_t>::max(), 32,
                              std::numeric_limits<uint16_t>::max(), 64,
                              std::numeric_limits<uint8_t>::max(),  128};
    dawn::RenderPipeline pipeline =
        MakeTestPipeline<uint32_t>(dawn::VertexFormat::UInt2, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

// Test that the Float4 vertex format is correctly interprete
TEST_P(VertexFormatTest, UInt3) {
    uint32_t vertexData[9] = {std::numeric_limits<uint32_t>::max(), 32,   64,
                              std::numeric_limits<uint16_t>::max(), 164,  128,
                              std::numeric_limits<uint8_t>::max(),  1283, 256};
    dawn::RenderPipeline pipeline =
        MakeTestPipeline<uint32_t>(dawn::VertexFormat::UInt3, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, UInt4) {
    uint32_t vertexData[12] = {std::numeric_limits<uint32_t>::max(), 32,   64,  5460,
                               std::numeric_limits<uint16_t>::max(), 164,  128, 0,
                               std::numeric_limits<uint8_t>::max(),  1283, 256, 4567};
    dawn::RenderPipeline pipeline =
        MakeTestPipeline<uint32_t>(dawn::VertexFormat::UInt4, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, Int) {
    int32_t vertexData[3] = {std::numeric_limits<int32_t>::max(),
                             std::numeric_limits<int32_t>::min(),
                             std::numeric_limits<int8_t>::max()};
    dawn::RenderPipeline pipeline = MakeTestPipeline<int32_t>(dawn::VertexFormat::Int, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

// Test that the Float4 vertex format is correctly interpreted
TEST_P(VertexFormatTest, Int2) {
    int32_t vertexData[6] = {
        std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min(),
        std::numeric_limits<int16_t>::max(), std::numeric_limits<int16_t>::min(),
        std::numeric_limits<int8_t>::max(),  std::numeric_limits<int8_t>::min()};
    dawn::RenderPipeline pipeline = MakeTestPipeline<int32_t>(dawn::VertexFormat::Int2, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

// Test that the Float4 vertex format is correctly interprete
TEST_P(VertexFormatTest, Int3) {
    int32_t vertexData[9] = {
        std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min(), 64,
        std::numeric_limits<int16_t>::max(), std::numeric_limits<int16_t>::min(), 128,
        std::numeric_limits<int8_t>::max(),  std::numeric_limits<int8_t>::min(),  256};
    dawn::RenderPipeline pipeline = MakeTestPipeline<int32_t>(dawn::VertexFormat::Int3, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

TEST_P(VertexFormatTest, Int4) {
    int32_t vertexData[12] = {
        std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min(), 64,   -5460,
        std::numeric_limits<int16_t>::max(), std::numeric_limits<int16_t>::min(), -128, 0,
        std::numeric_limits<int8_t>::max(),  std::numeric_limits<int8_t>::min(),  256,  -4567};
    dawn::RenderPipeline pipeline =
        MakeTestPipeline<int32_t>(dawn::VertexFormat::UInt4, vertexData);

    dawn::Buffer vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                            dawn::BufferUsageBit::Vertex);

    uint64_t zeroOffset = 0;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, &zeroOffset);
        pass.Draw(3, 1, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), renderPass.color, 0, 0);
}

DAWN_INSTANTIATE_TEST(VertexFormatTest, D3D12Backend, MetalBackend, OpenGLBackend, VulkanBackend);