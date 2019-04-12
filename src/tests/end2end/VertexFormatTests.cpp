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
#include "common/Math.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/DawnHelpers.h"

// Vertex format tests all work the same way: the test will render a triangle.
// Each test will set up a vertex buffer, and the vertex shader will check that
// the vertex content is the same as what we expected. On success it outputs green,
// otherwise red.

constexpr uint32_t kRTSize = 400;
constexpr uint32_t kVertexNum = 3;

template <typename T>
std::vector<float> Normalize(std::vector<T> data) {
    std::vector<float> expectedData;
    for (auto& element : data) {
        expectedData.push_back(Normalize(element));
    }
    return expectedData;
}

std::vector<float> ExtractFloat16toFloat32(std::vector<uint16_t> data) {
    std::vector<float> expectedData;
    for (auto& element : data) {
        expectedData.push_back(float16ToFloat32(element));
    }
    return expectedData;
}

template <typename T>
std::vector<float> BitCast(std::vector<T> data) {
    std::vector<float> expectedData;
    for (auto& element : data) {
        expectedData.push_back(bitCast<float>(element));
    }
    return expectedData;
}

class VertexFormatTest : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);
    }

    utils::BasicRenderPass renderPass;
    dawn::BindGroupLayout mBindGroupLayout;

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

    uint32_t ComponentCount(dawn::VertexFormat format) {
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
            default:
                DAWN_UNREACHABLE();
        }
    }

    std::string ShaderVariableType(bool isFloat,
                                   bool isNormalized,
                                   bool isUnsigned,
                                   uint32_t componentCount) {
        if (componentCount == 1) {
            if (isFloat || isNormalized) {
                return "float";
            } else if (isUnsigned) {
                return "uint";
            } else {
                return "int";
            }
        } else {
            if (isNormalized || isFloat) {
                return "vec" + std::to_string(componentCount);
            } else if (isUnsigned) {
                return "uvec" + std::to_string(componentCount);
            } else {
                return "ivec" + std::to_string(componentCount);
            }
        }
    }

    std::string ShaderExpectedDataType(bool isFloat, bool isNormalized, bool isUnsigned) {
        if (isFloat || isNormalized) {
            return "float";
        } else if (isUnsigned) {
            return "uint";
        } else {
            return "int";
        }
    }

    // The length of vertexData is fixed to 3, it aligns to triangle vertex number
    template <typename T>
    dawn::RenderPipeline MakeTestPipeline(dawn::VertexFormat format, std::vector<T>& expectedData) {
        bool isFloat = IsFloatFormat(format);
        bool isNormalized = IsNormalizedFormat(format);
        bool isUnsigned = IsUnsignedFormat(format);

        uint32_t componentCount = ComponentCount(format);

        std::string variableType =
            ShaderVariableType(isFloat, isNormalized, isUnsigned, componentCount);
        std::string expectedDataType = ShaderExpectedDataType(isFloat, isNormalized, isUnsigned);
        std::ostringstream vs;
        vs << "#version 450\n";

        // layout(location = 0) in float/uint/int/ivecn/vecn/uvecn test;
        vs << "layout(location = 0) in " << variableType << " test;\n";
        vs << "layout(location = 0) out vec4 color;\n";
        vs << "void main() {\n";

        // Hard code the triangle in the shader so that we don't have to add a vertex input for it.
        vs << "    const vec2 pos[3] = vec2[3](vec2(-1.0f, 0.0f), vec2(-1.0f, -1.0f), vec2(0.0f, "
              "-1.0f));\n";
        vs << "    gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);\n";

        // Declare expected values.
        vs << "    " << expectedDataType << " expected[" + std::to_string(kVertexNum) + "]";
        if (componentCount > 1) {
            vs << "[" + std::to_string(componentCount) + "];\n";
        } else {
            vs << ";\n";
        }
        // Assign each elements in expected values
        // e.g. expected[0][0] = uint(1);
        //      expected[0][1] = uint(2);
        if (componentCount > 1) {
            for (uint32_t i = 0; i < kVertexNum; ++i) {
                for (uint32_t j = 0; j < componentCount; ++j) {
                    vs << "    expected[" + std::to_string(i) + "][" + std::to_string(j) + "] = "
                       << expectedDataType << "(";
                    if (isFloat || isNormalized) {
                        // Whether it's a NaN.
                        if (expectedData[i * componentCount + j] ==
                            expectedData[i * componentCount + j]) {
                            vs << std::to_string(expectedData[i * componentCount + j]) << ");\n";
                        } else {
                            // Set NaN.
                            vs << "0.0 / 0.0);\n";
                        }
                    } else {
                        vs << std::to_string(expectedData[i * componentCount + j]) << ");\n";
                    }
                }
            }
        } else {
            for (uint32_t i = 0; i < kVertexNum; ++i) {
                vs << "    expected[" + std::to_string(i) + "] = " << expectedDataType << "(";
                if (isFloat || isNormalized) {
                    // Whether it's a NaN.
                    if (expectedData[i] == expectedData[i]) {
                        vs << std::to_string(expectedData[i]) << ");\n";
                    } else {
                        // Set NaN.
                        vs << "0.0 / 0.0);\n";
                    }
                } else {
                    vs << std::to_string(expectedData[i]) << ");\n";
                }
            }
        }

        vs << "    bool success = true;\n";

        // Perform the checks by successively ANDing a boolean
        vs << "    success = success";
        if (!isNormalized &&
            !isFloat) {  // For integer/unsigned integer, they need to equal perfectly.
            if (componentCount > 1) {
                for (uint32_t component = 0; component < componentCount; ++component) {
                    vs << " && test[" << component << "] == expected[gl_VertexIndex][" << component
                       << "]";
                }
            } else {
                vs << " && test == expected[gl_VertexIndex]";
            }
        } else {
            if (componentCount > 1) {  // For float, they need to be almost bits equal.
                for (uint32_t component = 0; component < componentCount; ++component) {
                    vs << " && abs(uint(test[" << component << "]) - uint(expected[gl_VertexIndex]["
                       << component << "])) < 1 ";
                }
            } else {
                vs << " && abs(uint(test) - uint(expected[gl_VertexIndex])) < 1";
            }
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
            ComponentCount(format) * BytesPerComponents(format);
        descriptor.cInputState.numAttributes = 1;
        descriptor.cInputState.cAttributes[0].format = format;
        descriptor.cColorStates[0]->format = renderPass.colorFormat;

        return device.CreateRenderPipeline(&descriptor);
    }

    template <typename VertexType, typename ExpectedType>
    void DoVertexFormatTest(dawn::VertexFormat format,
                            std::vector<VertexType> vertex,
                            std::vector<ExpectedType> expectedData) {
        dawn::RenderPipeline pipeline = MakeTestPipeline(format, expectedData);
        dawn::Buffer vertexBuffer =
            utils::CreateBufferFromData(device, vertex.data(), vertex.size() * sizeof(VertexType),
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
};

TEST_P(VertexFormatTest, UChar2) {
    std::vector<uint8_t> vertexData = {
        std::numeric_limits<uint8_t>::max(),
        0,
        std::numeric_limits<uint8_t>::min(),
        2,
        200,
        201,
        0,
        0  // padding two bytes for buffer copy
    };

    DoVertexFormatTest(dawn::VertexFormat::UChar2, vertexData, vertexData);
}

TEST_P(VertexFormatTest, UChar4) {
    std::vector<uint8_t> vertexData = {
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

    DoVertexFormatTest(dawn::VertexFormat::UChar4, vertexData, vertexData);
}

TEST_P(VertexFormatTest, Char2) {
    std::vector<int8_t> vertexData = {
        std::numeric_limits<int8_t>::max(),
        0,
        std::numeric_limits<int8_t>::min(),
        -2,
        120,
        -121,
        0,
        0  // padding two bytes for buffer copy
    };

    DoVertexFormatTest(dawn::VertexFormat::Char2, vertexData, vertexData);
}

TEST_P(VertexFormatTest, Char4) {
    std::vector<int8_t> vertexData = {
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

    DoVertexFormatTest(dawn::VertexFormat::Char4, vertexData, vertexData);
}

TEST_P(VertexFormatTest, UChar2Norm) {
    std::vector<uint8_t> vertexData = {
        std::numeric_limits<uint8_t>::max(),
        std::numeric_limits<uint8_t>::min(),
        std::numeric_limits<uint8_t>::max() / 2,
        std::numeric_limits<uint8_t>::min() / 2,
        200,
        201,
        0,
        0  // padding two bytes for buffer copy
    };

    DoVertexFormatTest(dawn::VertexFormat::UChar2Norm, vertexData, Normalize(vertexData));
}

TEST_P(VertexFormatTest, UChar4Norm) {
    std::vector<uint8_t> vertexData = {std::numeric_limits<uint8_t>::max(),
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

    DoVertexFormatTest(dawn::VertexFormat::UChar4Norm, vertexData, Normalize(vertexData));
}

TEST_P(VertexFormatTest, Char2Norm) {
    std::vector<int8_t> vertexData = {
        std::numeric_limits<int8_t>::max(),
        std::numeric_limits<int8_t>::min(),
        std::numeric_limits<int8_t>::max() / 2,
        std::numeric_limits<int8_t>::min() / 2,
        120,
        -121,
        0,
        0  // padding two bytes for buffer copy
    };

    DoVertexFormatTest(dawn::VertexFormat::Char2Norm, vertexData, Normalize(vertexData));
}

TEST_P(VertexFormatTest, Char4Norm) {
    std::vector<int8_t> vertexData = {std::numeric_limits<int8_t>::max(),
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

    DoVertexFormatTest(dawn::VertexFormat::Char4Norm, vertexData, Normalize(vertexData));
}

TEST_P(VertexFormatTest, UShort2) {
    std::vector<uint16_t> vertexData = {std::numeric_limits<uint16_t>::max(),
                                        0,
                                        std::numeric_limits<uint16_t>::min(),
                                        2,
                                        65432,
                                        4890};

    DoVertexFormatTest(dawn::VertexFormat::UShort2, vertexData, vertexData);
}

TEST_P(VertexFormatTest, UShort4) {
    std::vector<uint16_t> vertexData = {
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

    DoVertexFormatTest(dawn::VertexFormat::UShort4, vertexData, vertexData);
}

TEST_P(VertexFormatTest, Short2) {
    std::vector<int16_t> vertexData = {std::numeric_limits<int16_t>::max(),
                                       0,
                                       std::numeric_limits<int16_t>::min(),
                                       -2,
                                       3876,
                                       -3948};

    DoVertexFormatTest(dawn::VertexFormat::Short2, vertexData, vertexData);
}

TEST_P(VertexFormatTest, Short4) {
    std::vector<int16_t> vertexData = {
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

    DoVertexFormatTest(dawn::VertexFormat::Short4, vertexData, vertexData);
}

TEST_P(VertexFormatTest, UShort2Norm) {
    std::vector<uint16_t> vertexData = {std::numeric_limits<uint16_t>::max(),
                                        std::numeric_limits<uint16_t>::min(),
                                        std::numeric_limits<uint16_t>::max() / 2,
                                        std::numeric_limits<uint16_t>::min() / 2,
                                        3456,
                                        6543};

    DoVertexFormatTest(dawn::VertexFormat::UShort2Norm, vertexData, Normalize(vertexData));
}

TEST_P(VertexFormatTest, UShort4Norm) {
    std::vector<uint16_t> vertexData = {std::numeric_limits<uint16_t>::max(),
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

    DoVertexFormatTest(dawn::VertexFormat::UShort4Norm, vertexData, Normalize(vertexData));
}

TEST_P(VertexFormatTest, Short2Norm) {
    std::vector<int16_t> vertexData = {std::numeric_limits<int16_t>::max(),
                                       std::numeric_limits<int16_t>::min(),
                                       std::numeric_limits<int16_t>::max() / 2,
                                       std::numeric_limits<int16_t>::min() / 2,
                                       4987,
                                       -6789};

    DoVertexFormatTest(dawn::VertexFormat::Short2Norm, vertexData, Normalize(vertexData));
}

TEST_P(VertexFormatTest, Short4Norm) {
    std::vector<int16_t> vertexData = {std::numeric_limits<int16_t>::max(),
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

    DoVertexFormatTest(dawn::VertexFormat::Short4Norm, vertexData, Normalize(vertexData));
}

TEST_P(VertexFormatTest, Half2) {
    std::vector<uint16_t> vertexData = {
        float32ToFloat16(14.8),  float32ToFloat16(-12.4), float32ToFloat16(22.5),
        float32ToFloat16(-48.8), float32ToFloat16(47.4),  float32ToFloat16(-24.8),
    };

    std::vector<float> expectedData = {14.8f, -12.4f, 22.5f, -48.8f, 47.4f, -24.8f};

    DoVertexFormatTest(dawn::VertexFormat::Half2, vertexData, ExtractFloat16toFloat32(vertexData));
}

TEST_P(VertexFormatTest, Half4) {
    std::vector<uint16_t> vertexData = {
        float32ToFloat16(32.1),  float32ToFloat16(-16.8), float32ToFloat16(18.2),
        float32ToFloat16(-24.7), float32ToFloat16(12.5),  float32ToFloat16(-18.2),
        float32ToFloat16(14.8),  float32ToFloat16(-12.4), float32ToFloat16(22.5),
        float32ToFloat16(-48.8), float32ToFloat16(47.4),  float32ToFloat16(-24.8),
    };

    DoVertexFormatTest(dawn::VertexFormat::Half4, vertexData, ExtractFloat16toFloat32(vertexData));
}

TEST_P(VertexFormatTest, Float) {
    std::vector<float> vertexData = {0.0 / 0.0, +0.0f, -0.0f};

    std::vector<float> expectedData = {bitCast<float>(vertexData[0]), bitCast<float>(vertexData[1]),
                                       bitCast<float>(vertexData[2])};

    DoVertexFormatTest(dawn::VertexFormat::Float, vertexData, BitCast(vertexData));
}

TEST_P(VertexFormatTest, Float2) {
    std::vector<float> vertexData = {18.23f, -0.0f, +0.0f, 1.0f, 0.0 / 0.0, 1.6f};

    DoVertexFormatTest(dawn::VertexFormat::Float2, vertexData, BitCast(vertexData));
}

TEST_P(VertexFormatTest, Float3) {
    std::vector<float> vertexData = {
        +0.0f, -1.0f, -0.0f, 1.0f, 0.0 / 0.0, 99.45f, 23.6f, -81.2f, 55.0f,
    };

    DoVertexFormatTest(dawn::VertexFormat::Float3, vertexData, BitCast(vertexData));
}

TEST_P(VertexFormatTest, Float4) {
    std::vector<float> vertexData = {
        19.2f, -19.3f, +0.0f, 1.0f, -0.0f, 1.0f, 0.0 / 0.0, -1.0f, 13.078f, 21.1965f, -1.1f, -1.2f,
    };

    DoVertexFormatTest(dawn::VertexFormat::Float4, vertexData, BitCast(vertexData));
}

TEST_P(VertexFormatTest, UInt) {
    std::vector<uint32_t> vertexData = {std::numeric_limits<uint32_t>::max(),
                                        std::numeric_limits<uint16_t>::max(),
                                        std::numeric_limits<uint8_t>::max()};

    DoVertexFormatTest(dawn::VertexFormat::UInt, vertexData, vertexData);
}

TEST_P(VertexFormatTest, UInt2) {
    std::vector<uint32_t> vertexData = {std::numeric_limits<uint32_t>::max(), 32,
                                        std::numeric_limits<uint16_t>::max(), 64,
                                        std::numeric_limits<uint8_t>::max(),  128};

    DoVertexFormatTest(dawn::VertexFormat::UInt2, vertexData, vertexData);
}

TEST_P(VertexFormatTest, UInt3) {
    std::vector<uint32_t> vertexData = {std::numeric_limits<uint32_t>::max(), 32,   64,
                                        std::numeric_limits<uint16_t>::max(), 164,  128,
                                        std::numeric_limits<uint8_t>::max(),  1283, 256};

    DoVertexFormatTest(dawn::VertexFormat::UInt3, vertexData, vertexData);
}

TEST_P(VertexFormatTest, UInt4) {
    std::vector<uint32_t> vertexData = {std::numeric_limits<uint32_t>::max(), 32,   64,  5460,
                                        std::numeric_limits<uint16_t>::max(), 164,  128, 0,
                                        std::numeric_limits<uint8_t>::max(),  1283, 256, 4567};

    DoVertexFormatTest(dawn::VertexFormat::UInt4, vertexData, vertexData);
}

TEST_P(VertexFormatTest, Int) {
    std::vector<int32_t> vertexData = {std::numeric_limits<int32_t>::max(),
                                       std::numeric_limits<int32_t>::min(),
                                       std::numeric_limits<int8_t>::max()};

    DoVertexFormatTest(dawn::VertexFormat::Int, vertexData, vertexData);
}

TEST_P(VertexFormatTest, Int2) {
    std::vector<int32_t> vertexData = {
        std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min(),
        std::numeric_limits<int16_t>::max(), std::numeric_limits<int16_t>::min(),
        std::numeric_limits<int8_t>::max(),  std::numeric_limits<int8_t>::min()};

    DoVertexFormatTest(dawn::VertexFormat::Int2, vertexData, vertexData);
}

TEST_P(VertexFormatTest, Int3) {
    std::vector<int32_t> vertexData = {
        std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min(), 64,
        std::numeric_limits<int16_t>::max(), std::numeric_limits<int16_t>::min(), 128,
        std::numeric_limits<int8_t>::max(),  std::numeric_limits<int8_t>::min(),  256};

    DoVertexFormatTest(dawn::VertexFormat::Int3, vertexData, vertexData);
}

TEST_P(VertexFormatTest, Int4) {
    std::vector<int32_t> vertexData = {
        std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min(), 64,   -5460,
        std::numeric_limits<int16_t>::max(), std::numeric_limits<int16_t>::min(), -128, 0,
        std::numeric_limits<int8_t>::max(),  std::numeric_limits<int8_t>::min(),  256,  -4567};

    DoVertexFormatTest(dawn::VertexFormat::Int4, vertexData, vertexData);
}

DAWN_INSTANTIATE_TEST(VertexFormatTest, D3D12Backend, MetalBackend, OpenGLBackend, VulkanBackend);