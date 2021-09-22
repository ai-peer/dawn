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

#include <gtest/gtest.h>

#include "dawn_native/Limits.h"

// Test |GetDefaultLimits| returns the default.
TEST(Limits, GetDefaultLimits) {
    dawn_native::Limits limits = {};
    EXPECT_NE(limits.maxBindGroups, 4u);

    dawn_native::GetDefaultLimits(&limits);

    EXPECT_EQ(limits.maxBindGroups, 4u);
}

// Test |ReifyDefaultLimits| populates the default if
// values are undefined.
TEST(Limits, ReifyDefaultLimits_PopulatesDefault) {
    dawn_native::Limits limits;
    limits.maxComputeWorkgroupStorageSize = wgpu::kLimitU32Undefined;
    limits.maxStorageBufferBindingSize = wgpu::kLimitU64Undefined;

    dawn_native::Limits reified = dawn_native::ReifyDefaultLimits(limits);
    EXPECT_EQ(reified.maxComputeWorkgroupStorageSize, 16352u);
    EXPECT_EQ(reified.maxStorageBufferBindingSize, 134217728ul);
}

// Test |ReifyDefaultLimits| clamps to the default if
// values are worse than the default.
TEST(Limits, ReifyDefaultLimits_Clamps) {
    dawn_native::Limits limits;
    limits.maxStorageBuffersPerShaderStage = 4;
    limits.minUniformBufferOffsetAlignment = 512;

    dawn_native::Limits reified = dawn_native::ReifyDefaultLimits(limits);
    EXPECT_EQ(reified.maxStorageBuffersPerShaderStage, 8u);
    EXPECT_EQ(reified.minUniformBufferOffsetAlignment, 256u);
}

// Test |ValidateLimits| works to validate limits are not better
// than supported.
TEST(Limits, ValidateLimits) {
    // Start with the default for supported.
    dawn_native::Limits defaults;
    dawn_native::GetDefaultLimits(&defaults);

    // Test supported == required is valid.
    {
        dawn_native::Limits required = defaults;
        EXPECT_TRUE(ValidateLimits(defaults, required).IsSuccess());
    }

    // Test supported == required is valid, when they are not default.
    {
        dawn_native::Limits supported = defaults;
        dawn_native::Limits required = defaults;
        supported.maxBindGroups += 1;
        required.maxBindGroups += 1;
        EXPECT_TRUE(ValidateLimits(supported, required).IsSuccess());
    }

    // Test that default-initialized (all undefined) is valid.
    {
        dawn_native::Limits required = {};
        EXPECT_TRUE(ValidateLimits(defaults, required).IsSuccess());
    }

    // Test that better than max is invalid.
    {
        dawn_native::Limits required = {};
        required.maxTextureDimension3D = defaults.maxTextureDimension3D + 1;
        dawn_native::MaybeError err = ValidateLimits(defaults, required);
        EXPECT_TRUE(err.IsError());
        err.AcquireError();
    }

    // Test that worse than max is valid.
    {
        dawn_native::Limits required = {};
        required.maxComputeWorkgroupSizeX = defaults.maxComputeWorkgroupSizeX - 1;
        EXPECT_TRUE(ValidateLimits(defaults, required).IsSuccess());
    }

    // Test that better than min is invalid.
    {
        dawn_native::Limits required = {};
        required.minUniformBufferOffsetAlignment = defaults.minUniformBufferOffsetAlignment / 2;
        dawn_native::MaybeError err = ValidateLimits(defaults, required);
        EXPECT_TRUE(err.IsError());
        err.AcquireError();
    }

    // Test that worse than min is valid.
    {
        dawn_native::Limits required = {};
        required.minStorageBufferOffsetAlignment = defaults.minStorageBufferOffsetAlignment * 2;
        EXPECT_TRUE(ValidateLimits(defaults, required).IsSuccess());
    }
}

// Test that |ApplyLimitTiers| degrades limits to the next best tier.
TEST(Limits, ApplyLimitTiers) {
    auto SetLimitsMemorySizeTier2 = [](dawn_native::Limits* limits) {
        limits->maxTextureDimension1D = 8192;
        limits->maxTextureDimension2D = 8192;
        limits->maxTextureDimension3D = 4096;
        limits->maxTextureArrayLayers = 1024;
        limits->maxUniformBufferBindingSize = 65536;
        limits->maxStorageBufferBindingSize = 1073741824;
        limits->maxComputeWorkgroupStorageSize = 32768;
    };
    dawn_native::Limits limitsMemorySizeTier2;
    dawn_native::GetDefaultLimits(&limitsMemorySizeTier2);
    SetLimitsMemorySizeTier2(&limitsMemorySizeTier2);

    auto SetLimitsMemorySizeTier3 = [](dawn_native::Limits* limits) {
        limits->maxTextureDimension1D = 16384;
        limits->maxTextureDimension2D = 16384;
        limits->maxTextureDimension3D = 8192;
        limits->maxTextureArrayLayers = 2048;
        limits->maxUniformBufferBindingSize = 134218000;
        limits->maxStorageBufferBindingSize = 2147483647;
        limits->maxComputeWorkgroupStorageSize = 49152;
    };
    dawn_native::Limits limitsMemorySizeTier3;
    dawn_native::GetDefaultLimits(&limitsMemorySizeTier3);
    SetLimitsMemorySizeTier3(&limitsMemorySizeTier3);

    auto SetLimitsBindingSpaceTier1 = [](dawn_native::Limits* limits) {
        limits->maxBindGroups = 4;
        limits->maxDynamicUniformBuffersPerPipelineLayout = 8;
        limits->maxDynamicStorageBuffersPerPipelineLayout = 4;
        limits->maxSampledTexturesPerShaderStage = 16;
        limits->maxSamplersPerShaderStage = 16;
        limits->maxStorageBuffersPerShaderStage = 8;
        limits->maxStorageTexturesPerShaderStage = 4;
        limits->maxUniformBuffersPerShaderStage = 12;
    };
    dawn_native::Limits limitsBindingSpaceTier1;
    dawn_native::GetDefaultLimits(&limitsBindingSpaceTier1);
    SetLimitsBindingSpaceTier1(&limitsBindingSpaceTier1);

    auto SetLimitsBindingSpaceTier3 = [](dawn_native::Limits* limits) {
        limits->maxBindGroups = 32;
        limits->maxDynamicUniformBuffersPerPipelineLayout = 32;
        limits->maxDynamicStorageBuffersPerPipelineLayout = 16;
        limits->maxSampledTexturesPerShaderStage = 64;
        limits->maxSamplersPerShaderStage = 64;
        limits->maxStorageBuffersPerShaderStage = 32;
        limits->maxStorageTexturesPerShaderStage = 16;
        limits->maxUniformBuffersPerShaderStage = 48;
    };
    dawn_native::Limits limitsBindingSpaceTier3;
    dawn_native::GetDefaultLimits(&limitsBindingSpaceTier3);
    SetLimitsBindingSpaceTier3(&limitsBindingSpaceTier3);

    // Test that applying tiers to limits that are exactly
    // equal to a tier returns the same values.
    {
        dawn_native::Limits limits = limitsMemorySizeTier2;
        EXPECT_EQ(ApplyLimitTiers(limits), limits);

        limits = limitsMemorySizeTier3;
        EXPECT_EQ(ApplyLimitTiers(limits), limits);
    }

    // Test all limits slightly worse than tier 3.
    {
        dawn_native::Limits limits = limitsMemorySizeTier3;
        limits.maxTextureDimension1D -= 1;
        limits.maxTextureDimension2D -= 1;
        limits.maxTextureDimension3D -= 1;
        limits.maxTextureArrayLayers -= 1;
        limits.maxUniformBufferBindingSize -= 1;
        limits.maxStorageBufferBindingSize -= 1;
        limits.maxComputeWorkgroupStorageSize -= 1;
        EXPECT_EQ(ApplyLimitTiers(limits), limitsMemorySizeTier2);
    }

    // Test that any limit worse than tier 3 degrades all limits to tier 2.
    {
        dawn_native::Limits limits = limitsMemorySizeTier3;
        limits.maxTextureArrayLayers -= 1;
        EXPECT_EQ(ApplyLimitTiers(limits), limitsMemorySizeTier2);
    }

    // Test that limits may match one tier exactly and be degraded in another tier.
    // Degrading to one tier does not affect the other tier.
    {
        dawn_native::Limits limits = limitsBindingSpaceTier3;
        // Set tier 3 and change one limit to be insufficent.
        SetLimitsMemorySizeTier3(&limits);
        limits.maxTextureDimension1D -= 1;

        dawn_native::Limits tiered = ApplyLimitTiers(limits);

        // Check that |tiered| has the limits of memorySize tier 2
        dawn_native::Limits tieredWithMemorySizeTier2 = tiered;
        SetLimitsMemorySizeTier2(&tieredWithMemorySizeTier2);
        EXPECT_EQ(tiered, tieredWithMemorySizeTier2);

        // Check that |tiered| has the limits of bindingSpace tier 3
        dawn_native::Limits tieredWithBindingSpaceTier3 = tiered;
        SetLimitsBindingSpaceTier3(&tieredWithBindingSpaceTier3);
        EXPECT_EQ(tiered, tieredWithBindingSpaceTier3);
    }

    // Test that limits may be simultaneously degraded in two tiers independently.
    {
        dawn_native::Limits limits;
        dawn_native::GetDefaultLimits(&limits);
        SetLimitsBindingSpaceTier3(&limits);
        SetLimitsMemorySizeTier3(&limits);
        limits.maxBindGroups = 5;  // Good enough for binding space tier 1, but not 2.
        limits.maxComputeWorkgroupStorageSize =
            49151;  // Good enough for memory size tier 2, but not 3.

        dawn_native::Limits tiered = ApplyLimitTiers(limits);

        dawn_native::Limits expected = tiered;
        SetLimitsBindingSpaceTier1(&expected);
        SetLimitsMemorySizeTier2(&expected);
        EXPECT_EQ(tiered, expected);
    }
}
