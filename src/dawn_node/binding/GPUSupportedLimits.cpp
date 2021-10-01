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

#include "src/dawn_node/binding/GPUSupportedLimits.h"
#include "src/common/Constants.h"

namespace wgpu { namespace binding {

    ////////////////////////////////////////////////////////////////////////////////
    // wgpu::bindings::GPUSupportedLimits
    ////////////////////////////////////////////////////////////////////////////////

    // TODO(crbug.com/dawn/1131): Once the Dawn API exposes these values, use them instead of
    // including Constants.h

    uint32_t GPUSupportedLimits::getMaxTextureDimension1D(Napi::Env) {
        return kMaxTextureDimension1D;
    }

    uint32_t GPUSupportedLimits::getMaxTextureDimension2D(Napi::Env) {
        return kMaxTextureDimension2D;
    }

    uint32_t GPUSupportedLimits::getMaxTextureDimension3D(Napi::Env) {
        return kMaxTextureDimension3D;
    }

    uint32_t GPUSupportedLimits::getMaxTextureArrayLayers(Napi::Env) {
        return kMaxTextureArrayLayers;
    }

    uint32_t GPUSupportedLimits::getMaxBindGroups(Napi::Env) {
        return kMaxBindGroups;
    }

    uint32_t GPUSupportedLimits::getMaxDynamicUniformBuffersPerPipelineLayout(Napi::Env) {
        return kMaxDynamicUniformBuffersPerPipelineLayout;
    }

    uint32_t GPUSupportedLimits::getMaxDynamicStorageBuffersPerPipelineLayout(Napi::Env) {
        return kMaxDynamicStorageBuffersPerPipelineLayout;
    }

    uint32_t GPUSupportedLimits::getMaxSampledTexturesPerShaderStage(Napi::Env) {
        return kMaxSampledTexturesPerShaderStage;
    }

    uint32_t GPUSupportedLimits::getMaxSamplersPerShaderStage(Napi::Env) {
        return kMaxSamplersPerShaderStage;
    }

    uint32_t GPUSupportedLimits::getMaxStorageBuffersPerShaderStage(Napi::Env) {
        return kMaxStorageBuffersPerShaderStage;
    }

    uint32_t GPUSupportedLimits::getMaxStorageTexturesPerShaderStage(Napi::Env) {
        return kMaxStorageTexturesPerShaderStage;
    }

    uint32_t GPUSupportedLimits::getMaxUniformBuffersPerShaderStage(Napi::Env) {
        return kMaxUniformBuffersPerShaderStage;
    }

    uint64_t GPUSupportedLimits::getMaxUniformBufferBindingSize(Napi::Env) {
        return kMaxUniformBufferBindingSize;
    }

    uint64_t GPUSupportedLimits::getMaxStorageBufferBindingSize(Napi::Env) {
        return kMaxStorageBufferBindingSize;
    }

    uint32_t GPUSupportedLimits::getMinUniformBufferOffsetAlignment(Napi::Env) {
        return kMinUniformBufferOffsetAlignment;
    }

    uint32_t GPUSupportedLimits::getMinStorageBufferOffsetAlignment(Napi::Env) {
        return kMinStorageBufferOffsetAlignment;
    }

    uint32_t GPUSupportedLimits::getMaxVertexBuffers(Napi::Env) {
        return kMaxVertexBuffers;
    }

    uint32_t GPUSupportedLimits::getMaxVertexAttributes(Napi::Env) {
        return kMaxVertexAttributes;
    }

    uint32_t GPUSupportedLimits::getMaxVertexBufferArrayStride(Napi::Env) {
        return kMaxVertexBufferArrayStride;
    }

    uint32_t GPUSupportedLimits::getMaxInterStageShaderComponents(Napi::Env) {
        return kMaxInterStageShaderComponents;
    }

    uint32_t GPUSupportedLimits::getMaxComputeWorkgroupStorageSize(Napi::Env) {
        return kMaxComputeWorkgroupStorageSize;
    }

    uint32_t GPUSupportedLimits::getMaxComputeInvocationsPerWorkgroup(Napi::Env) {
        return kMaxComputeWorkgroupInvocations;
    }

    uint32_t GPUSupportedLimits::getMaxComputeWorkgroupSizeX(Napi::Env) {
        return kMaxComputeWorkgroupSizeX;
    }

    uint32_t GPUSupportedLimits::getMaxComputeWorkgroupSizeY(Napi::Env) {
        return kMaxComputeWorkgroupSizeY;
    }

    uint32_t GPUSupportedLimits::getMaxComputeWorkgroupSizeZ(Napi::Env) {
        return kMaxComputeWorkgroupSizeZ;
    }

    uint32_t GPUSupportedLimits::getMaxComputeWorkgroupsPerDimension(Napi::Env) {
        return kMaxComputePerDimensionDispatchSize;
    }

}}  // namespace wgpu::binding
