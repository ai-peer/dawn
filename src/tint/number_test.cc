// Copyright 2022 The Tint Authors.
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

#include <cmath>
#include <tuple>
#include <vector>

#include "src/tint/program_builder.h"
#include "src/tint/utils/compiler_macros.h"

#include "gtest/gtest.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint {
namespace {

constexpr int64_t kHighestI32 = static_cast<int64_t>(std::numeric_limits<int32_t>::max());
constexpr int64_t kHighestU32 = static_cast<int64_t>(std::numeric_limits<uint32_t>::max());
constexpr int64_t kLowestI32 = static_cast<int64_t>(std::numeric_limits<int32_t>::min());
constexpr int64_t kLowestU32 = static_cast<int64_t>(std::numeric_limits<uint32_t>::min());

// Highest float32 value.
constexpr double kHighestF32 = 0x1.fffffep+127;

// Next ULP up from kHighestF32 for a float64.
constexpr double kHighestF32NextULP = 0x1.fffffe0000001p+127;

// Smallest positive normal float32 value.
constexpr double kSmallestF32 = 0x1p-126;

// Highest subnormal value for a float32.
constexpr double kHighestF32Subnormal = 0x0.fffffep-126;

// Highest float16 value.
constexpr double kHighestF16 = 0x1.ffcp+15;

// Next ULP up from kHighestF16 for a float64.
constexpr double kHighestF16NextULP = 0x1.ffc0000000001p+15;

// Smallest positive normal float16 value.
constexpr double kSmallestF16 = 0x1p-14;

// Highest subnormal value for a float16.
constexpr double kHighestF16Subnormal = 0x0.ffcp-14;

constexpr double kLowestF32 = -kHighestF32;
constexpr double kLowestF32NextULP = -kHighestF32NextULP;
constexpr double kLowestF16 = -kHighestF16;
constexpr double kLowestF16NextULP = -kHighestF16NextULP;

// MSVC (only in release builds) can grumble about some of the inlined numerical overflow /
// underflow that's done in this file. We like to think we know what we're doing, so silence the
// warning.
TINT_BEGIN_DISABLE_WARNING(CONSTANT_OVERFLOW);

TEST(NumberTest, CheckedConvertIdentity) {
    EXPECT_EQ(CheckedConvert<AInt>(0_a), 0_a);
    EXPECT_EQ(CheckedConvert<AFloat>(0_a), 0.0_a);
    EXPECT_EQ(CheckedConvert<i32>(0_i), 0_i);
    EXPECT_EQ(CheckedConvert<u32>(0_u), 0_u);
    EXPECT_EQ(CheckedConvert<f32>(0_f), 0_f);
    EXPECT_EQ(CheckedConvert<f16>(0_h), 0_h);

    EXPECT_EQ(CheckedConvert<AInt>(1_a), 1_a);
    EXPECT_EQ(CheckedConvert<AFloat>(1_a), 1.0_a);
    EXPECT_EQ(CheckedConvert<i32>(1_i), 1_i);
    EXPECT_EQ(CheckedConvert<u32>(1_u), 1_u);
    EXPECT_EQ(CheckedConvert<f32>(1_f), 1_f);
    EXPECT_EQ(CheckedConvert<f16>(1_h), 1_h);
}

TEST(NumberTest, CheckedConvertLargestValue) {
    EXPECT_EQ(CheckedConvert<i32>(AInt(kHighestI32)), i32(kHighestI32));
    EXPECT_EQ(CheckedConvert<u32>(AInt(kHighestU32)), u32(kHighestU32));
    EXPECT_EQ(CheckedConvert<f32>(AFloat(kHighestF32)), f32(kHighestF32));
    EXPECT_EQ(CheckedConvert<f16>(AFloat(kHighestF16)), f16(kHighestF16));
}

TEST(NumberTest, CheckedConvertLowestValue) {
    EXPECT_EQ(CheckedConvert<i32>(AInt(kLowestI32)), i32(kLowestI32));
    EXPECT_EQ(CheckedConvert<u32>(AInt(kLowestU32)), u32(kLowestU32));
    EXPECT_EQ(CheckedConvert<f32>(AFloat(kLowestF32)), f32(kLowestF32));
    EXPECT_EQ(CheckedConvert<f16>(AFloat(kLowestF16)), f16(kLowestF16));
}

TEST(NumberTest, CheckedConvertSmallestValue) {
    EXPECT_EQ(CheckedConvert<i32>(AInt(0)), i32(0));
    EXPECT_EQ(CheckedConvert<u32>(AInt(0)), u32(0));
    EXPECT_EQ(CheckedConvert<f32>(AFloat(kSmallestF32)), f32(kSmallestF32));
    EXPECT_EQ(CheckedConvert<f16>(AFloat(kSmallestF16)), f16(kSmallestF16));
}

TEST(NumberTest, CheckedConvertExceedsPositiveLimit) {
    EXPECT_EQ(CheckedConvert<i32>(AInt(kHighestI32 + 1)), ConversionFailure::kExceedsPositiveLimit);
    EXPECT_EQ(CheckedConvert<u32>(AInt(kHighestU32 + 1)), ConversionFailure::kExceedsPositiveLimit);
    EXPECT_EQ(CheckedConvert<f32>(AFloat(kHighestF32NextULP)),
              ConversionFailure::kExceedsPositiveLimit);
    EXPECT_EQ(CheckedConvert<f16>(AFloat(kHighestF16NextULP)),
              ConversionFailure::kExceedsPositiveLimit);
}

TEST(NumberTest, CheckedConvertExceedsNegativeLimit) {
    EXPECT_EQ(CheckedConvert<i32>(AInt(kLowestI32 - 1)), ConversionFailure::kExceedsNegativeLimit);
    EXPECT_EQ(CheckedConvert<u32>(AInt(kLowestU32 - 1)), ConversionFailure::kExceedsNegativeLimit);
    EXPECT_EQ(CheckedConvert<f32>(AFloat(kLowestF32NextULP)),
              ConversionFailure::kExceedsNegativeLimit);
    EXPECT_EQ(CheckedConvert<f16>(AFloat(kLowestF16NextULP)),
              ConversionFailure::kExceedsNegativeLimit);
}

TEST(NumberTest, CheckedConvertSubnormals) {
    EXPECT_EQ(CheckedConvert<f32>(AFloat(kHighestF32Subnormal)), f32(kHighestF32Subnormal));
    EXPECT_EQ(CheckedConvert<f16>(AFloat(kHighestF16Subnormal)), f16(kHighestF16Subnormal));
    EXPECT_EQ(CheckedConvert<f32>(AFloat(-kHighestF32Subnormal)), f32(-kHighestF32Subnormal));
    EXPECT_EQ(CheckedConvert<f16>(AFloat(-kHighestF16Subnormal)), f16(-kHighestF16Subnormal));
}

// Test cases for f16 subnormal quantization and BitsRepresentation.
// The ULP is based on float rather than double or f16, since F16::Quantize and
// F16::BitsRepresentation take float as input.
constexpr float lowestPositiveNormalF16 = 0x1p-14;
constexpr float lowestPositiveNormalF16PlusULP = 0x1.000002p-14;
constexpr float lowestPositiveNormalF16MinusULP = 0x1.fffffep-15;
constexpr float highestPositiveSubnormalF16 = 0x0.ffcp-14;
constexpr float highestPositiveSubnormalF16PlusULP = 0x1.ff8002p-15;
constexpr float highestPositiveSubnormalF16MinusULP = 0x1.ff7ffep-15;
constexpr float lowestPositiveSubnormalF16 = 0x1.p-24;
constexpr float lowestPositiveSubnormalF16PlusULP = 0x1.000002p-24;
constexpr float lowestPositiveSubnormalF16MinusULP = 0x1.fffffep-25;

constexpr uint16_t lowestPositiveNormalF16Bits = 0x0400u;
constexpr uint16_t highestPositiveSubnormalF16Bits = 0x03ffu;
constexpr uint16_t lowestPositiveSubnormalF16Bits = 0x0001u;

constexpr float highestNegativeNormalF16 = -lowestPositiveNormalF16;
constexpr float highestNegativeNormalF16PlusULP = -lowestPositiveNormalF16MinusULP;
constexpr float highestNegativeNormalF16MinusULP = -lowestPositiveNormalF16PlusULP;
constexpr float lowestNegativeSubnormalF16 = -highestPositiveSubnormalF16;
constexpr float lowestNegativeSubnormalF16PlusULP = -highestPositiveSubnormalF16MinusULP;
constexpr float lowestNegativeSubnormalF16MinusULP = -highestPositiveSubnormalF16PlusULP;
constexpr float highestNegativeSubnormalF16 = -lowestPositiveSubnormalF16;
constexpr float highestNegativeSubnormalF16PlusULP = -lowestPositiveSubnormalF16MinusULP;
constexpr float highestNegativeSubnormalF16MinusULP = -lowestPositiveSubnormalF16PlusULP;

constexpr uint16_t highestNegativeNormalF16Bits = 0x8400u;
constexpr uint16_t lowestNegativeSubnormalF16Bits = 0x83ffu;
constexpr uint16_t highestNegativeSubnormalF16Bits = 0x8001u;

TEST(NumberTest, QuantizeF16) {
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float inf = std::numeric_limits<float>::infinity();

    EXPECT_EQ(f16(0.0), 0.0f);
    EXPECT_EQ(f16(1.0), 1.0f);
    EXPECT_EQ(f16(0.00006106496), 0.000061035156f);
    EXPECT_EQ(f16(1.0004883), 1.0f);
    EXPECT_EQ(f16(-8196), -8192.f);
    EXPECT_EQ(f16(65504.003), inf);
    EXPECT_EQ(f16(-65504.003), -inf);
    EXPECT_EQ(f16(inf), inf);
    EXPECT_EQ(f16(-inf), -inf);
    EXPECT_TRUE(std::isnan(f16(nan)));

    // Test for subnormal quantization.
    // Value larger than or equal to lowest positive normal f16 will be quantized to normal f16.
    EXPECT_EQ(f16(lowestPositiveNormalF16PlusULP), lowestPositiveNormalF16);
    EXPECT_EQ(f16(lowestPositiveNormalF16), lowestPositiveNormalF16);
    // Positive value smaller than lowest positive normal f16 but not smaller than lowest positive
    // subnormal f16 will be quantized to subnormal f16 or zero.
    EXPECT_EQ(f16(lowestPositiveNormalF16MinusULP), highestPositiveSubnormalF16);
    EXPECT_EQ(f16(highestPositiveSubnormalF16PlusULP), highestPositiveSubnormalF16);
    EXPECT_EQ(f16(highestPositiveSubnormalF16), highestPositiveSubnormalF16);
    EXPECT_EQ(f16(highestPositiveSubnormalF16MinusULP), 0x0.ff8p-14);
    EXPECT_EQ(f16(lowestPositiveSubnormalF16PlusULP), lowestPositiveSubnormalF16);
    EXPECT_EQ(f16(lowestPositiveSubnormalF16), lowestPositiveSubnormalF16);
    // Positive value smaller than lowest positive subnormal f16 will be quantized to zero.
    EXPECT_EQ(f16(lowestPositiveSubnormalF16MinusULP), 0.0);
    // Test the mantissa discarding, the least significant mantissa bit is 0x1p-24 = 0x0.004p-14.
    EXPECT_EQ(f16(0x0.064p-14), 0x0.064p-14);
    EXPECT_EQ(f16(0x0.067fecp-14), 0x0.064p-14);
    EXPECT_EQ(f16(0x0.063ffep-14), 0x0.060p-14);
    EXPECT_EQ(f16(0x0.008p-14), 0x0.008p-14);
    EXPECT_EQ(f16(0x0.00bffep-14), 0x0.008p-14);
    EXPECT_EQ(f16(0x0.007ffep-14), 0x0.004p-14);

    // Vice versa for negative cases.
    EXPECT_EQ(f16(highestNegativeNormalF16MinusULP), highestNegativeNormalF16);
    EXPECT_EQ(f16(highestNegativeNormalF16), highestNegativeNormalF16);
    EXPECT_EQ(f16(highestNegativeNormalF16PlusULP), lowestNegativeSubnormalF16);
    EXPECT_EQ(f16(lowestNegativeSubnormalF16MinusULP), lowestNegativeSubnormalF16);
    EXPECT_EQ(f16(lowestNegativeSubnormalF16), lowestNegativeSubnormalF16);
    EXPECT_EQ(f16(lowestNegativeSubnormalF16PlusULP), -0x0.ff8p-14);
    EXPECT_EQ(f16(highestNegativeSubnormalF16MinusULP), highestNegativeSubnormalF16);
    EXPECT_EQ(f16(highestNegativeSubnormalF16), highestNegativeSubnormalF16);
    EXPECT_EQ(f16(highestNegativeSubnormalF16PlusULP), 0.0);

    // Test the mantissa discarding.
    EXPECT_EQ(f16(-0x0.064p-14), -0x0.064p-14);
    EXPECT_EQ(f16(-0x0.067fecp-14), -0x0.064p-14);
    EXPECT_EQ(f16(-0x0.063ffep-14), -0x0.060p-14);
    EXPECT_EQ(f16(-0x0.008p-14), -0x0.008p-14);
    EXPECT_EQ(f16(-0x0.00bffep-14), -0x0.008p-14);
    EXPECT_EQ(f16(-0x0.007ffep-14), -0x0.004p-14);
}

TEST(NumberTest, F16BitsRepresentation_static) {
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float inf = std::numeric_limits<float>::infinity();

    // NaN, inf
    EXPECT_EQ(f16::BitsRepresentation(inf), 0x7c00u);
    EXPECT_EQ(f16::BitsRepresentation(-inf), 0xfc00u);
    EXPECT_EQ(f16::BitsRepresentation(nan), 0x7e00u);
    EXPECT_EQ(f16::BitsRepresentation(-nan), 0x7e00u);
    // +/- zero
    EXPECT_EQ(f16::BitsRepresentation(+0.0), 0x0000u);
    EXPECT_EQ(f16::BitsRepresentation(-0.0), 0x8000u);
    // Value in normal f16 range
    EXPECT_EQ(f16::BitsRepresentation(1.0), 0x3c00u);
    EXPECT_EQ(f16::BitsRepresentation(-1.0), 0xbc00u);
    //   0.00006106496 quantized to 0.000061035156 = 0x1p-14
    EXPECT_EQ(f16::BitsRepresentation(0.00006106496), 0x0400u);
    EXPECT_EQ(f16::BitsRepresentation(-0.00006106496), 0x8400u);
    //   1.0004883 quantized to 1.0 = 0x1p0
    EXPECT_EQ(f16::BitsRepresentation(1.0004883), 0x3c00u);
    EXPECT_EQ(f16::BitsRepresentation(-1.0004883), 0xbc00u);
    //   8196.0 quantized to 8192.0 = 0x1p13
    EXPECT_EQ(f16::BitsRepresentation(-8196), 0xf000u);
    EXPECT_EQ(f16::BitsRepresentation(8196), 0x7000u);
    // Value in subnormal f16 range
    EXPECT_EQ(f16::BitsRepresentation(0x0.034p-14), 0x000du);
    EXPECT_EQ(f16::BitsRepresentation(-0x0.034p-14), 0x800du);
    EXPECT_EQ(f16::BitsRepresentation(0x0.068p-14), 0x001au);
    EXPECT_EQ(f16::BitsRepresentation(-0x0.068p-14), 0x801au);
    //   0x0.06b7p-14 quantized to 0x0.068p-14
    EXPECT_EQ(f16::BitsRepresentation(0x0.06b7p-14), 0x001au);
    EXPECT_EQ(f16::BitsRepresentation(-0x0.06b7p-14), 0x801au);
    // Value out of f16 range
    EXPECT_EQ(f16::BitsRepresentation(65504.003), 0x7c00u);
    EXPECT_EQ(f16::BitsRepresentation(-65504.003), 0xfc00u);
    EXPECT_EQ(f16::BitsRepresentation(0x1.234p56), 0x7c00u);
    EXPECT_EQ(f16::BitsRepresentation(-0x4.321p65), 0xfc00u);

    // Test for subnormal quantization.
    // Value larger than or equal to lowest positive normal f16 will be quantized to normal f16.
    EXPECT_EQ(f16::BitsRepresentation(lowestPositiveNormalF16PlusULP), lowestPositiveNormalF16Bits);
    EXPECT_EQ(f16::BitsRepresentation(lowestPositiveNormalF16), lowestPositiveNormalF16Bits);
    // Positive value smaller than lowest positive normal f16 but not smaller than lowest positive
    // subnormal f16 will be quantized to subnormal f16 or zero.
    EXPECT_EQ(f16::BitsRepresentation(lowestPositiveNormalF16MinusULP),
              highestPositiveSubnormalF16Bits);
    EXPECT_EQ(f16::BitsRepresentation(highestPositiveSubnormalF16PlusULP),
              highestPositiveSubnormalF16Bits);
    EXPECT_EQ(f16::BitsRepresentation(highestPositiveSubnormalF16),
              highestPositiveSubnormalF16Bits);
    EXPECT_EQ(f16::BitsRepresentation(highestPositiveSubnormalF16MinusULP), 0x03feu);
    EXPECT_EQ(f16::BitsRepresentation(lowestPositiveSubnormalF16PlusULP),
              lowestPositiveSubnormalF16Bits);
    EXPECT_EQ(f16::BitsRepresentation(lowestPositiveSubnormalF16), lowestPositiveSubnormalF16Bits);
    // Positive value smaller than lowest positive subnormal f16 will be quantized to zero.
    EXPECT_EQ(f16::BitsRepresentation(lowestPositiveSubnormalF16MinusULP), 0x0000u);
    // Test the mantissa discarding, the least significant mantissa bit is 0x1p-24 = 0x0.004p-14.
    EXPECT_EQ(f16::BitsRepresentation(0x0.064p-14), 0x0019u);
    EXPECT_EQ(f16::BitsRepresentation(0x0.067fecp-14), 0x0019u);
    EXPECT_EQ(f16::BitsRepresentation(0x0.063ffep-14), 0x0018u);
    EXPECT_EQ(f16::BitsRepresentation(0x0.008p-14), 0x0002u);
    EXPECT_EQ(f16::BitsRepresentation(0x0.00bffep-14), 0x0002u);
    EXPECT_EQ(f16::BitsRepresentation(0x0.007ffep-14), 0x0001u);

    // Vice versa for negative cases.
    EXPECT_EQ(f16::BitsRepresentation(highestNegativeNormalF16MinusULP),
              highestNegativeNormalF16Bits);
    EXPECT_EQ(f16::BitsRepresentation(highestNegativeNormalF16), highestNegativeNormalF16Bits);
    EXPECT_EQ(f16::BitsRepresentation(highestNegativeNormalF16PlusULP),
              lowestNegativeSubnormalF16Bits);
    EXPECT_EQ(f16::BitsRepresentation(lowestNegativeSubnormalF16MinusULP),
              lowestNegativeSubnormalF16Bits);
    EXPECT_EQ(f16::BitsRepresentation(lowestNegativeSubnormalF16), lowestNegativeSubnormalF16Bits);
    EXPECT_EQ(f16::BitsRepresentation(lowestNegativeSubnormalF16PlusULP), 0x83feu);
    EXPECT_EQ(f16::BitsRepresentation(highestNegativeSubnormalF16MinusULP),
              highestNegativeSubnormalF16Bits);
    EXPECT_EQ(f16::BitsRepresentation(highestNegativeSubnormalF16),
              highestNegativeSubnormalF16Bits);
    EXPECT_EQ(f16::BitsRepresentation(highestNegativeSubnormalF16PlusULP), 0x8000u);
    // Test the mantissa discarding.
    EXPECT_EQ(f16::BitsRepresentation(-0x0.064p-14), 0x8019u);
    EXPECT_EQ(f16::BitsRepresentation(-0x0.067fecp-14), 0x8019u);
    EXPECT_EQ(f16::BitsRepresentation(-0x0.063ffep-14), 0x8018u);
    EXPECT_EQ(f16::BitsRepresentation(-0x0.008p-14), 0x8002u);
    EXPECT_EQ(f16::BitsRepresentation(-0x0.00bffep-14), 0x8002u);
    EXPECT_EQ(f16::BitsRepresentation(-0x0.007ffep-14), 0x8001u);
}

TEST(NumberTest, F16BitsRepresentation_member) {
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float inf = std::numeric_limits<float>::infinity();

    // NaN, inf
    EXPECT_EQ(f16(inf).BitsRepresentation(), 0x7c00u);
    EXPECT_EQ(f16(-inf).BitsRepresentation(), 0xfc00u);
    EXPECT_EQ(f16(nan).BitsRepresentation(), 0x7e00u);
    EXPECT_EQ(f16(-nan).BitsRepresentation(), 0x7e00u);
    // +/- zero
    EXPECT_EQ(f16(+0.0).BitsRepresentation(), 0x0000u);
    EXPECT_EQ(f16(-0.0).BitsRepresentation(), 0x8000u);
    // Value in normal f16 range
    EXPECT_EQ(f16(1.0).BitsRepresentation(), 0x3c00u);
    EXPECT_EQ(f16(-1.0).BitsRepresentation(), 0xbc00u);
    //   0.00006106496 quantized to 0.000061035156 = 0x1p-14
    EXPECT_EQ(f16(0.00006106496).BitsRepresentation(), 0x0400u);
    EXPECT_EQ(f16(-0.00006106496).BitsRepresentation(), 0x8400u);
    //   1.0004883 quantized to 1.0 = 0x1p0
    EXPECT_EQ(f16(1.0004883).BitsRepresentation(), 0x3c00u);
    EXPECT_EQ(f16(-1.0004883).BitsRepresentation(), 0xbc00u);
    //   8196.0 quantized to 8192.0 = 0x1p13
    EXPECT_EQ(f16(-8196).BitsRepresentation(), 0xf000u);
    EXPECT_EQ(f16(8196).BitsRepresentation(), 0x7000u);
    // Value in subnormal f16 range
    EXPECT_EQ(f16(0x0.034p-14).BitsRepresentation(), 0x000du);
    EXPECT_EQ(f16(-0x0.034p-14).BitsRepresentation(), 0x800du);
    EXPECT_EQ(f16(0x0.068p-14).BitsRepresentation(), 0x001au);
    EXPECT_EQ(f16(-0x0.068p-14).BitsRepresentation(), 0x801au);
    //   0x0.06b7p-14 quantized to 0x0.068p-14
    EXPECT_EQ(f16(0x0.06b7p-14).BitsRepresentation(), 0x001au);
    EXPECT_EQ(f16(-0x0.06b7p-14).BitsRepresentation(), 0x801au);
    // Value out of f16 range
    EXPECT_EQ(f16(65504.003).BitsRepresentation(), 0x7c00u);
    EXPECT_EQ(f16(-65504.003).BitsRepresentation(), 0xfc00u);
    EXPECT_EQ(f16(0x1.234p56).BitsRepresentation(), 0x7c00u);
    EXPECT_EQ(f16(-0x4.321p65).BitsRepresentation(), 0xfc00u);

    // Test for subnormal quantization.
    // Value larger than or equal to lowest positive normal f16 will be quantized to normal f16.
    EXPECT_EQ(f16(lowestPositiveNormalF16PlusULP).BitsRepresentation(),
              lowestPositiveNormalF16Bits);
    EXPECT_EQ(f16(lowestPositiveNormalF16).BitsRepresentation(), lowestPositiveNormalF16Bits);
    // Positive value smaller than lowest positive normal f16 but not smaller than lowest positive
    // subnormal f16 will be quantized to subnormal f16 or zero.
    EXPECT_EQ(f16(lowestPositiveNormalF16MinusULP).BitsRepresentation(),
              highestPositiveSubnormalF16Bits);
    EXPECT_EQ(f16(highestPositiveSubnormalF16PlusULP).BitsRepresentation(),
              highestPositiveSubnormalF16Bits);
    EXPECT_EQ(f16(highestPositiveSubnormalF16).BitsRepresentation(),
              highestPositiveSubnormalF16Bits);
    EXPECT_EQ(f16(highestPositiveSubnormalF16MinusULP).BitsRepresentation(), 0x03feu);
    EXPECT_EQ(f16(lowestPositiveSubnormalF16PlusULP).BitsRepresentation(),
              lowestPositiveSubnormalF16Bits);
    EXPECT_EQ(f16(lowestPositiveSubnormalF16).BitsRepresentation(), lowestPositiveSubnormalF16Bits);
    // Positive value smaller than lowest positive subnormal f16 will be quantized to zero.
    EXPECT_EQ(f16(lowestPositiveSubnormalF16MinusULP).BitsRepresentation(), 0x0000u);
    // Test the mantissa discarding, the least significant mantissa bit is 0x1p-24 = 0x0.004p-14.
    EXPECT_EQ(f16(0x0.064p-14).BitsRepresentation(), 0x0019u);
    EXPECT_EQ(f16(0x0.067fecp-14).BitsRepresentation(), 0x0019u);
    EXPECT_EQ(f16(0x0.063ffep-14).BitsRepresentation(), 0x0018u);
    EXPECT_EQ(f16(0x0.008p-14).BitsRepresentation(), 0x0002u);
    EXPECT_EQ(f16(0x0.00bffep-14).BitsRepresentation(), 0x0002u);
    EXPECT_EQ(f16(0x0.007ffep-14).BitsRepresentation(), 0x0001u);

    // Vice versa for negative cases.
    EXPECT_EQ(f16(highestNegativeNormalF16MinusULP).BitsRepresentation(),
              highestNegativeNormalF16Bits);
    EXPECT_EQ(f16(highestNegativeNormalF16).BitsRepresentation(), highestNegativeNormalF16Bits);
    EXPECT_EQ(f16(highestNegativeNormalF16PlusULP).BitsRepresentation(),
              lowestNegativeSubnormalF16Bits);
    EXPECT_EQ(f16(lowestNegativeSubnormalF16MinusULP).BitsRepresentation(),
              lowestNegativeSubnormalF16Bits);
    EXPECT_EQ(f16(lowestNegativeSubnormalF16).BitsRepresentation(), lowestNegativeSubnormalF16Bits);
    EXPECT_EQ(f16(lowestNegativeSubnormalF16PlusULP).BitsRepresentation(), 0x83feu);
    EXPECT_EQ(f16(highestNegativeSubnormalF16MinusULP).BitsRepresentation(),
              highestNegativeSubnormalF16Bits);
    EXPECT_EQ(f16(highestNegativeSubnormalF16).BitsRepresentation(),
              highestNegativeSubnormalF16Bits);
    EXPECT_EQ(f16(highestNegativeSubnormalF16PlusULP).BitsRepresentation(), 0x8000u);
    // Test the mantissa discarding.
    EXPECT_EQ(f16(-0x0.064p-14).BitsRepresentation(), 0x8019u);
    EXPECT_EQ(f16(-0x0.067fecp-14).BitsRepresentation(), 0x8019u);
    EXPECT_EQ(f16(-0x0.063ffep-14).BitsRepresentation(), 0x8018u);
    EXPECT_EQ(f16(-0x0.008p-14).BitsRepresentation(), 0x8002u);
    EXPECT_EQ(f16(-0x0.00bffep-14).BitsRepresentation(), 0x8002u);
    EXPECT_EQ(f16(-0x0.007ffep-14).BitsRepresentation(), 0x8001u);
}

using BinaryCheckedCase = std::tuple<std::optional<AInt>, AInt, AInt>;

#undef OVERFLOW  // corecrt_math.h :(
#define OVERFLOW \
    {}

using CheckedAddTest = testing::TestWithParam<BinaryCheckedCase>;
TEST_P(CheckedAddTest, Test) {
    auto expect = std::get<0>(GetParam());
    auto a = std::get<1>(GetParam());
    auto b = std::get<2>(GetParam());
    EXPECT_EQ(CheckedAdd(a, b), expect) << std::hex << "0x" << a << " * 0x" << b;
    EXPECT_EQ(CheckedAdd(b, a), expect) << std::hex << "0x" << a << " * 0x" << b;
}
INSTANTIATE_TEST_SUITE_P(
    CheckedAddTest,
    CheckedAddTest,
    testing::ValuesIn(std::vector<BinaryCheckedCase>{
        {AInt(0), AInt(0), AInt(0)},
        {AInt(1), AInt(1), AInt(0)},
        {AInt(2), AInt(1), AInt(1)},
        {AInt(0), AInt(-1), AInt(1)},
        {AInt(3), AInt(2), AInt(1)},
        {AInt(-1), AInt(-2), AInt(1)},
        {AInt(0x300), AInt(0x100), AInt(0x200)},
        {AInt(0x100), AInt(-0x100), AInt(0x200)},
        {AInt(AInt::kHighest), AInt(1), AInt(AInt::kHighest - 1)},
        {AInt(AInt::kLowest), AInt(-1), AInt(AInt::kLowest + 1)},
        {AInt(AInt::kHighest), AInt(0x7fffffff00000000ll), AInt(0x00000000ffffffffll)},
        {AInt(AInt::kHighest), AInt(AInt::kHighest), AInt(0)},
        {AInt(AInt::kLowest), AInt(AInt::kLowest), AInt(0)},
        {OVERFLOW, AInt(1), AInt(AInt::kHighest)},
        {OVERFLOW, AInt(-1), AInt(AInt::kLowest)},
        {OVERFLOW, AInt(2), AInt(AInt::kHighest)},
        {OVERFLOW, AInt(-2), AInt(AInt::kLowest)},
        {OVERFLOW, AInt(10000), AInt(AInt::kHighest)},
        {OVERFLOW, AInt(-10000), AInt(AInt::kLowest)},
        {OVERFLOW, AInt(AInt::kHighest), AInt(AInt::kHighest)},
        {OVERFLOW, AInt(AInt::kLowest), AInt(AInt::kLowest)},
        ////////////////////////////////////////////////////////////////////////
    }));

using CheckedMulTest = testing::TestWithParam<BinaryCheckedCase>;
TEST_P(CheckedMulTest, Test) {
    auto expect = std::get<0>(GetParam());
    auto a = std::get<1>(GetParam());
    auto b = std::get<2>(GetParam());
    EXPECT_EQ(CheckedMul(a, b), expect) << std::hex << "0x" << a << " * 0x" << b;
    EXPECT_EQ(CheckedMul(b, a), expect) << std::hex << "0x" << a << " * 0x" << b;
}
INSTANTIATE_TEST_SUITE_P(
    CheckedMulTest,
    CheckedMulTest,
    testing::ValuesIn(std::vector<BinaryCheckedCase>{
        {AInt(0), AInt(0), AInt(0)},
        {AInt(0), AInt(1), AInt(0)},
        {AInt(1), AInt(1), AInt(1)},
        {AInt(-1), AInt(-1), AInt(1)},
        {AInt(2), AInt(2), AInt(1)},
        {AInt(-2), AInt(-2), AInt(1)},
        {AInt(0x20000), AInt(0x100), AInt(0x200)},
        {AInt(-0x20000), AInt(-0x100), AInt(0x200)},
        {AInt(0x4000000000000000ll), AInt(0x80000000ll), AInt(0x80000000ll)},
        {AInt(0x4000000000000000ll), AInt(-0x80000000ll), AInt(-0x80000000ll)},
        {AInt(0x1000000000000000ll), AInt(0x40000000ll), AInt(0x40000000ll)},
        {AInt(-0x1000000000000000ll), AInt(-0x40000000ll), AInt(0x40000000ll)},
        {AInt(0x100000000000000ll), AInt(0x1000000), AInt(0x100000000ll)},
        {AInt(0x2000000000000000ll), AInt(0x1000000000000000ll), AInt(2)},
        {AInt(-0x2000000000000000ll), AInt(0x1000000000000000ll), AInt(-2)},
        {AInt(-0x2000000000000000ll), AInt(-0x1000000000000000ll), AInt(2)},
        {AInt(-0x2000000000000000ll), AInt(0x1000000000000000ll), AInt(-2)},
        {AInt(0x4000000000000000ll), AInt(0x1000000000000000ll), AInt(4)},
        {AInt(-0x4000000000000000ll), AInt(0x1000000000000000ll), AInt(-4)},
        {AInt(-0x4000000000000000ll), AInt(-0x1000000000000000ll), AInt(4)},
        {AInt(-0x4000000000000000ll), AInt(0x1000000000000000ll), AInt(-4)},
        {AInt(-0x8000000000000000ll), AInt(0x1000000000000000ll), AInt(-8)},
        {AInt(-0x8000000000000000ll), AInt(-0x1000000000000000ll), AInt(8)},
        {AInt(0), AInt(AInt::kHighest), AInt(0)},
        {AInt(0), AInt(AInt::kLowest), AInt(0)},
        {OVERFLOW, AInt(0x1000000000000000ll), AInt(8)},
        {OVERFLOW, AInt(-0x1000000000000000ll), AInt(-8)},
        {OVERFLOW, AInt(0x800000000000000ll), AInt(0x10)},
        {OVERFLOW, AInt(0x80000000ll), AInt(0x100000000ll)},
        {OVERFLOW, AInt(AInt::kHighest), AInt(AInt::kHighest)},
        {OVERFLOW, AInt(AInt::kHighest), AInt(AInt::kLowest)},
        ////////////////////////////////////////////////////////////////////////
    }));

using TernaryCheckedCase = std::tuple<std::optional<AInt>, AInt, AInt, AInt>;

using CheckedMaddTest = testing::TestWithParam<TernaryCheckedCase>;
TEST_P(CheckedMaddTest, Test) {
    auto expect = std::get<0>(GetParam());
    auto a = std::get<1>(GetParam());
    auto b = std::get<2>(GetParam());
    auto c = std::get<3>(GetParam());
    EXPECT_EQ(CheckedMadd(a, b, c), expect)
        << std::hex << "0x" << a << " * 0x" << b << " + 0x" << c;
    EXPECT_EQ(CheckedMadd(b, a, c), expect)
        << std::hex << "0x" << a << " * 0x" << b << " + 0x" << c;
}
INSTANTIATE_TEST_SUITE_P(
    CheckedMaddTest,
    CheckedMaddTest,
    testing::ValuesIn(std::vector<TernaryCheckedCase>{
        {AInt(0), AInt(0), AInt(0), AInt(0)},
        {AInt(0), AInt(1), AInt(0), AInt(0)},
        {AInt(1), AInt(1), AInt(1), AInt(0)},
        {AInt(2), AInt(1), AInt(1), AInt(1)},
        {AInt(0), AInt(1), AInt(-1), AInt(1)},
        {AInt(-1), AInt(1), AInt(-2), AInt(1)},
        {AInt(-1), AInt(-1), AInt(1), AInt(0)},
        {AInt(2), AInt(2), AInt(1), AInt(0)},
        {AInt(-2), AInt(-2), AInt(1), AInt(0)},
        {AInt(0), AInt(AInt::kHighest), AInt(0), AInt(0)},
        {AInt(0), AInt(AInt::kLowest), AInt(0), AInt(0)},
        {AInt(3), AInt(1), AInt(2), AInt(1)},
        {AInt(0x300), AInt(1), AInt(0x100), AInt(0x200)},
        {AInt(0x100), AInt(1), AInt(-0x100), AInt(0x200)},
        {AInt(0x20000), AInt(0x100), AInt(0x200), AInt(0)},
        {AInt(-0x20000), AInt(-0x100), AInt(0x200), AInt(0)},
        {AInt(0x4000000000000000ll), AInt(0x80000000ll), AInt(0x80000000ll), AInt(0)},
        {AInt(0x4000000000000000ll), AInt(-0x80000000ll), AInt(-0x80000000ll), AInt(0)},
        {AInt(0x1000000000000000ll), AInt(0x40000000ll), AInt(0x40000000ll), AInt(0)},
        {AInt(-0x1000000000000000ll), AInt(-0x40000000ll), AInt(0x40000000ll), AInt(0)},
        {AInt(0x100000000000000ll), AInt(0x1000000), AInt(0x100000000ll), AInt(0)},
        {AInt(0x2000000000000000ll), AInt(0x1000000000000000ll), AInt(2), AInt(0)},
        {AInt(-0x2000000000000000ll), AInt(0x1000000000000000ll), AInt(-2), AInt(0)},
        {AInt(-0x2000000000000000ll), AInt(-0x1000000000000000ll), AInt(2), AInt(0)},
        {AInt(-0x2000000000000000ll), AInt(0x1000000000000000ll), AInt(-2), AInt(0)},
        {AInt(0x4000000000000000ll), AInt(0x1000000000000000ll), AInt(4), AInt(0)},
        {AInt(-0x4000000000000000ll), AInt(0x1000000000000000ll), AInt(-4), AInt(0)},
        {AInt(-0x4000000000000000ll), AInt(-0x1000000000000000ll), AInt(4), AInt(0)},
        {AInt(-0x4000000000000000ll), AInt(0x1000000000000000ll), AInt(-4), AInt(0)},
        {AInt(-0x8000000000000000ll), AInt(0x1000000000000000ll), AInt(-8), AInt(0)},
        {AInt(-0x8000000000000000ll), AInt(-0x1000000000000000ll), AInt(8), AInt(0)},
        {AInt(AInt::kHighest), AInt(1), AInt(1), AInt(AInt::kHighest - 1)},
        {AInt(AInt::kLowest), AInt(1), AInt(-1), AInt(AInt::kLowest + 1)},
        {AInt(AInt::kHighest), AInt(1), AInt(0x7fffffff00000000ll), AInt(0x00000000ffffffffll)},
        {AInt(AInt::kHighest), AInt(1), AInt(AInt::kHighest), AInt(0)},
        {AInt(AInt::kLowest), AInt(1), AInt(AInt::kLowest), AInt(0)},
        {OVERFLOW, AInt(0x1000000000000000ll), AInt(8), AInt(0)},
        {OVERFLOW, AInt(-0x1000000000000000ll), AInt(-8), AInt(0)},
        {OVERFLOW, AInt(0x800000000000000ll), AInt(0x10), AInt(0)},
        {OVERFLOW, AInt(0x80000000ll), AInt(0x100000000ll), AInt(0)},
        {OVERFLOW, AInt(AInt::kHighest), AInt(AInt::kHighest), AInt(0)},
        {OVERFLOW, AInt(AInt::kHighest), AInt(AInt::kLowest), AInt(0)},
        {OVERFLOW, AInt(1), AInt(1), AInt(AInt::kHighest)},
        {OVERFLOW, AInt(1), AInt(-1), AInt(AInt::kLowest)},
        {OVERFLOW, AInt(1), AInt(2), AInt(AInt::kHighest)},
        {OVERFLOW, AInt(1), AInt(-2), AInt(AInt::kLowest)},
        {OVERFLOW, AInt(1), AInt(10000), AInt(AInt::kHighest)},
        {OVERFLOW, AInt(1), AInt(-10000), AInt(AInt::kLowest)},
        {OVERFLOW, AInt(1), AInt(AInt::kHighest), AInt(AInt::kHighest)},
        {OVERFLOW, AInt(1), AInt(AInt::kLowest), AInt(AInt::kLowest)},
        {OVERFLOW, AInt(1), AInt(AInt::kHighest), AInt(1)},
        {OVERFLOW, AInt(1), AInt(AInt::kLowest), AInt(-1)},
    }));

TINT_END_DISABLE_WARNING(CONSTANT_OVERFLOW);

}  // namespace
}  // namespace tint
