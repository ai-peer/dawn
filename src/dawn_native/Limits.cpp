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

#include "dawn_native/Limits.h"

#include "common/Assert.h"

#define LIMITS(X)                                      \
    X(>, maxTextureDimension1D, 8192)                  \
    X(>, maxTextureDimension2D, 8192)                  \
    X(>, maxTextureDimension3D, 2048)                  \
    X(>, maxTextureArrayLayers, 256)                   \
    X(>, maxBindGroups, 4)                             \
    X(>, maxDynamicUniformBuffersPerPipelineLayout, 8) \
    X(>, maxDynamicStorageBuffersPerPipelineLayout, 4) \
    X(>, maxSampledTexturesPerShaderStage, 16)         \
    X(>, maxSamplersPerShaderStage, 16)                \
    X(>, maxStorageBuffersPerShaderStage, 8)           \
    X(>, maxStorageTexturesPerShaderStage, 4)          \
    X(>, maxUniformBuffersPerShaderStage, 12)          \
    X(>, maxUniformBufferBindingSize, 16384)           \
    X(>, maxStorageBufferBindingSize, 134217728)       \
    X(<, minUniformBufferOffsetAlignment, 256)         \
    X(<, minStorageBufferOffsetAlignment, 256)         \
    X(>, maxVertexBuffers, 8)                          \
    X(>, maxVertexAttributes, 16)                      \
    X(>, maxVertexBufferArrayStride, 2048)             \
    X(>, maxInterStageShaderComponents, 60)            \
    X(>, maxComputeWorkgroupStorageSize, 16352)        \
    X(>, maxComputeInvocationsPerWorkgroup, 256)       \
    X(>, maxComputeWorkgroupSizeX, 256)                \
    X(>, maxComputeWorkgroupSizeY, 256)                \
    X(>, maxComputeWorkgroupSizeZ, 64)                 \
    X(>, maxComputeWorkgroupsPerDimension, 65535)

namespace dawn_native {
    namespace {
        template <const char BetterOp>
        struct CheckLimit;

        template <>
        struct CheckLimit<'<'> {
            template <typename T>
            static MaybeError Invoke(T supported, T required) {
                if (required < supported) {
                    return DAWN_VALIDATION_ERROR("requiredLimit lower than supported limit");
                }
                return {};
            }
        };

        template <>
        struct CheckLimit<'>'> {
            template <typename T>
            static MaybeError Invoke(T supported, T required) {
                if (required > supported) {
                    return DAWN_VALIDATION_ERROR("requiredLimit greater than supported limit");
                }
                return {};
            }
        };

        template <typename T>
        bool IsLimitUndefined(T value) {
            static_assert(sizeof(T) != sizeof(T), "IsLimitUndefined not implemented for this type");
            return false;
        }

        template <>
        bool IsLimitUndefined<uint32_t>(uint32_t value) {
            return value == wgpu::kLimitU32Undefined;
        }

        template <>
        bool IsLimitUndefined<uint64_t>(uint64_t value) {
            return value == wgpu::kLimitU64Undefined;
        }

    }  // namespace

    void GetDefaultLimits(Limits* limits) {
        ASSERT(limits != nullptr);
#define X(BetterOp, limitName, defaultValue) limits->limitName = defaultValue;
        LIMITS(X)
#undef X
    }

    Limits ReifyDefaultLimits(const Limits& limits) {
        Limits out;
#define X(BetterOp, limitName, defaultValue)                                            \
    if (IsLimitUndefined(limits.limitName) || defaultValue BetterOp limits.limitName) { \
        /* If the limit is undefined or the default is better, use the default */       \
        out.limitName = defaultValue;                                                   \
    } else {                                                                            \
        out.limitName = limits.limitName;                                               \
    }
        LIMITS(X)
#undef X
        return out;
    }

    MaybeError ValidateLimits(const Limits& supportedLimits, const Limits& requiredLimits) {
#define X(BetterOp, limitName, defaultValue)                                    \
    if (!IsLimitUndefined(requiredLimits.limitName)) {                          \
        DAWN_TRY(CheckLimit<(#BetterOp)[0]>::Invoke(supportedLimits.limitName,  \
                                                    requiredLimits.limitName)); \
    }
        LIMITS(X)
#undef X
        return {};
    }

}  // namespace dawn_native
