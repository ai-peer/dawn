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

#include <cstring>
#include <map>

#include "dawn/common/Assert.h"
#include "dawn/common/vulkan_platform.h"
#include "dawn/native/CacheKey.h"
#include "dawn/native/vulkan/RenderPassCache.h"

#include "icd/generated/vk_typemap_helper.h"

namespace dawn::native {

namespace {

namespace detail {

template <typename... VK_STRUCT_TYPES>
void ValidatePnextImpl(const VkBaseOutStructure* root) {
    const VkBaseOutStructure* next = reinterpret_cast<const VkBaseOutStructure*>(root->pNext);
    while (next != nullptr) {
        // Assert that the type of each pNext struct is exactly one of the specified
        // templates.
        ASSERT(((LvlTypeMap<VK_STRUCT_TYPES>::kSType == next->sType ? 1 : 0) + ... + 0) == 1);
        next = reinterpret_cast<const VkBaseOutStructure*>(next->pNext);
    }
}

template <typename VK_STRUCT_TYPE>
void SerializePnextImpl(serde::Sink* sink, const VkBaseOutStructure* root) {
    const VkBaseOutStructure* next = reinterpret_cast<const VkBaseOutStructure*>(root->pNext);
    const VK_STRUCT_TYPE* found = nullptr;
    while (next != nullptr) {
        if (LvlTypeMap<VK_STRUCT_TYPE>::kSType == next->sType) {
            if (found == nullptr) {
                found = reinterpret_cast<const VK_STRUCT_TYPE*>(next);
            } else {
                // Fail an assert here since that means that the chain had more than one of
                // the same typed chained object.
                ASSERT(false);
            }
        }
        next = reinterpret_cast<const VkBaseOutStructure*>(next->pNext);
    }
    if (found != nullptr) {
        Serialize(sink, found);
    }
}

template <typename VK_STRUCT_TYPE,
          typename... VK_STRUCT_TYPES,
          typename = std::enable_if_t<(sizeof...(VK_STRUCT_TYPES) > 0)>>
void SerializePnextImpl(serde::Sink* sink, const VkBaseOutStructure* root) {
    SerializePnextImpl<VK_STRUCT_TYPE>(sink, root);
    SerializePnextImpl<VK_STRUCT_TYPES...>(sink, root);
}

template <typename VK_STRUCT_TYPE>
const VkBaseOutStructure* ToVkBaseOutStructure(const VK_STRUCT_TYPE* t) {
    // Checks to ensure proper type safety.
    static_assert(offsetof(VK_STRUCT_TYPE, sType) == offsetof(VkBaseOutStructure, sType) &&
                      offsetof(VK_STRUCT_TYPE, pNext) == offsetof(VkBaseOutStructure, pNext),
                  "Argument type is not a proper Vulkan structure type");
    return reinterpret_cast<const VkBaseOutStructure*>(t);
}

}  // namespace detail

template <typename... VK_STRUCT_TYPES,
          typename VK_STRUCT_TYPE,
          typename = std::enable_if_t<(sizeof...(VK_STRUCT_TYPES) > 0)>>
void SerializePnext(serde::Sink* sink, const VK_STRUCT_TYPE* t) {
    const VkBaseOutStructure* root = detail::ToVkBaseOutStructure(t);
    detail::ValidatePnextImpl<VK_STRUCT_TYPES...>(root);
    detail::SerializePnextImpl<VK_STRUCT_TYPES...>(sink, root);
}

// Empty template specialization so that we can put this in to ensure failures occur if new
// extensions are added without updating serialization.
template <typename VK_STRUCT_TYPE>
void SerializePnext(serde::Sink* sink, const VK_STRUCT_TYPE* t) {
    const VkBaseOutStructure* root = detail::ToVkBaseOutStructure(t);
    detail::ValidatePnextImpl<>(root);
}

}  // namespace

template <>
void serde::Serde<VkDescriptorSetLayoutBinding>::SerializeImpl(
    serde::Sink* sink,
    const VkDescriptorSetLayoutBinding& t) {
    Serialize(sink, t.binding, t.descriptorType, t.descriptorCount, t.stageFlags);
}

template <>
void serde::Serde<VkDescriptorSetLayoutCreateInfo>::SerializeImpl(
    serde::Sink* sink,
    const VkDescriptorSetLayoutCreateInfo& t) {
    Serialize(sink, t.flags, Iterable(t.pBindings, t.bindingCount));
    SerializePnext(sink, &t);
}

template <>
void serde::Serde<VkPushConstantRange>::SerializeImpl(serde::Sink* sink,
                                                      const VkPushConstantRange& t) {
    Serialize(sink, t.stageFlags, t.offset, t.size);
}

template <>
void serde::Serde<VkPipelineLayoutCreateInfo>::SerializeImpl(serde::Sink* sink,
                                                             const VkPipelineLayoutCreateInfo& t) {
    // The set layouts are not serialized here because they are pointers to backend objects.
    // They need to be cross-referenced with the frontend objects and serialized from there.
    Serialize(sink, t.flags, Iterable(t.pPushConstantRanges, t.pushConstantRangeCount));
    SerializePnext(sink, &t);
}

template <>
void serde::Serde<VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT>::SerializeImpl(
    serde::Sink* sink,
    const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT& t) {
    Serialize(sink, t.requiredSubgroupSize);
}

template <>
void serde::Serde<VkSpecializationMapEntry>::SerializeImpl(serde::Sink* sink,
                                                           const VkSpecializationMapEntry& t) {
    Serialize(sink, t.constantID, t.offset, t.size);
}

template <>
void serde::Serde<VkSpecializationInfo>::SerializeImpl(serde::Sink* sink,
                                                       const VkSpecializationInfo& t) {
    Serialize(sink, Iterable(t.pMapEntries, t.mapEntryCount),
              Iterable(static_cast<const uint8_t*>(t.pData), t.dataSize));
}

template <>
void serde::Serde<VkPipelineShaderStageCreateInfo>::SerializeImpl(
    serde::Sink* sink,
    const VkPipelineShaderStageCreateInfo& t) {
    // The shader module is not serialized here because it is a pointer to a backend object.
    Serialize(sink, t.flags, t.stage, Iterable(t.pName, strlen(t.pName)), t.pSpecializationInfo);
    SerializePnext<VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT>(sink, &t);
}

template <>
void serde::Serde<VkComputePipelineCreateInfo>::SerializeImpl(
    serde::Sink* sink,
    const VkComputePipelineCreateInfo& t) {
    // The pipeline layout is not serialized here because it is a pointer to a backend object.
    // It needs to be cross-referenced with the frontend objects and serialized from there. The
    // base pipeline information is also currently not Serializeed since we do not use them in our
    // backend implementation. If we decide to use them later on, they also need to be
    // cross-referenced from the frontend.
    Serialize(sink, t.flags, t.stage);
}

template <>
void serde::Serde<VkVertexInputBindingDescription>::SerializeImpl(
    serde::Sink* sink,
    const VkVertexInputBindingDescription& t) {
    Serialize(sink, t.binding, t.stride, t.inputRate);
}

template <>
void serde::Serde<VkVertexInputAttributeDescription>::SerializeImpl(
    serde::Sink* sink,
    const VkVertexInputAttributeDescription& t) {
    Serialize(sink, t.location, t.binding, t.format, t.offset);
}

template <>
void serde::Serde<VkPipelineVertexInputStateCreateInfo>::SerializeImpl(
    serde::Sink* sink,
    const VkPipelineVertexInputStateCreateInfo& t) {
    Serialize(sink, t.flags,
              Iterable(t.pVertexBindingDescriptions, t.vertexBindingDescriptionCount),
              Iterable(t.pVertexAttributeDescriptions, t.vertexAttributeDescriptionCount));
    SerializePnext(sink, &t);
}

template <>
void serde::Serde<VkPipelineInputAssemblyStateCreateInfo>::SerializeImpl(
    serde::Sink* sink,
    const VkPipelineInputAssemblyStateCreateInfo& t) {
    Serialize(sink, t.flags, t.topology, t.primitiveRestartEnable);
    SerializePnext(sink, &t);
}

template <>
void serde::Serde<VkPipelineTessellationStateCreateInfo>::SerializeImpl(
    serde::Sink* sink,
    const VkPipelineTessellationStateCreateInfo& t) {
    Serialize(sink, t.flags, t.patchControlPoints);
    SerializePnext(sink, &t);
}

template <>
void serde::Serde<VkViewport>::SerializeImpl(serde::Sink* sink, const VkViewport& t) {
    Serialize(sink, t.x, t.y, t.width, t.height, t.minDepth, t.maxDepth);
}

template <>
void serde::Serde<VkOffset2D>::SerializeImpl(serde::Sink* sink, const VkOffset2D& t) {
    Serialize(sink, t.x, t.y);
}

template <>
void serde::Serde<VkExtent2D>::SerializeImpl(serde::Sink* sink, const VkExtent2D& t) {
    Serialize(sink, t.width, t.height);
}

template <>
void serde::Serde<VkRect2D>::SerializeImpl(serde::Sink* sink, const VkRect2D& t) {
    Serialize(sink, t.offset, t.extent);
}

template <>
void serde::Serde<VkPipelineViewportStateCreateInfo>::SerializeImpl(
    serde::Sink* sink,
    const VkPipelineViewportStateCreateInfo& t) {
    Serialize(sink, t.flags, Iterable(t.pViewports, t.viewportCount),
              Iterable(t.pScissors, t.scissorCount));
    SerializePnext(sink, &t);
}

template <>
void serde::Serde<VkPipelineRasterizationStateCreateInfo>::SerializeImpl(
    serde::Sink* sink,
    const VkPipelineRasterizationStateCreateInfo& t) {
    Serialize(sink, t.flags, t.depthClampEnable, t.rasterizerDiscardEnable, t.polygonMode,
              t.cullMode, t.frontFace, t.depthBiasEnable, t.depthBiasConstantFactor,
              t.depthBiasClamp, t.depthBiasSlopeFactor, t.lineWidth);
    SerializePnext(sink, &t);
}

template <>
void serde::Serde<VkPipelineMultisampleStateCreateInfo>::SerializeImpl(
    serde::Sink* sink,
    const VkPipelineMultisampleStateCreateInfo& t) {
    Serialize(sink, t.flags, t.rasterizationSamples, t.sampleShadingEnable, t.minSampleShading,
              t.pSampleMask, t.alphaToCoverageEnable, t.alphaToOneEnable);
    SerializePnext(sink, &t);
}

template <>
void serde::Serde<VkStencilOpState>::SerializeImpl(serde::Sink* sink, const VkStencilOpState& t) {
    Serialize(sink, t.failOp, t.passOp, t.depthFailOp, t.compareOp, t.compareMask, t.writeMask,
              t.reference);
}

template <>
void serde::Serde<VkPipelineDepthStencilStateCreateInfo>::SerializeImpl(
    serde::Sink* sink,
    const VkPipelineDepthStencilStateCreateInfo& t) {
    Serialize(sink, t.flags, t.depthTestEnable, t.depthWriteEnable, t.depthCompareOp,
              t.depthBoundsTestEnable, t.stencilTestEnable, t.front, t.back, t.minDepthBounds,
              t.maxDepthBounds);
    SerializePnext(sink, &t);
}

template <>
void serde::Serde<VkPipelineColorBlendAttachmentState>::SerializeImpl(
    serde::Sink* sink,
    const VkPipelineColorBlendAttachmentState& t) {
    Serialize(sink, t.blendEnable, t.srcColorBlendFactor, t.dstColorBlendFactor, t.colorBlendOp,
              t.srcAlphaBlendFactor, t.dstAlphaBlendFactor, t.alphaBlendOp, t.colorWriteMask);
}

template <>
void serde::Serde<VkPipelineColorBlendStateCreateInfo>::SerializeImpl(
    serde::Sink* sink,
    const VkPipelineColorBlendStateCreateInfo& t) {
    Serialize(sink, t.flags, t.logicOpEnable, t.logicOp,
              Iterable(t.pAttachments, t.attachmentCount), t.blendConstants);
    SerializePnext(sink, &t);
}

template <>
void serde::Serde<VkPipelineDynamicStateCreateInfo>::SerializeImpl(
    serde::Sink* sink,
    const VkPipelineDynamicStateCreateInfo& t) {
    Serialize(sink, t.flags, Iterable(t.pDynamicStates, t.dynamicStateCount));
    SerializePnext(sink, &t);
}

template <>
void serde::Serde<vulkan::RenderPassCacheQuery>::SerializeImpl(
    serde::Sink* sink,
    const vulkan::RenderPassCacheQuery& t) {
    Serialize(sink, t.colorMask.to_ulong(), t.resolveTargetMask.to_ulong(), t.sampleCount);

    // Manually iterate the color attachment indices and their corresponding format/load/store
    // ops because the data is sparse and may be uninitialized. Since we Serialize the colorMask
    // member above, Serializeing sparse data should be fine here.
    for (ColorAttachmentIndex i : IterateBitSet(t.colorMask)) {
        Serialize(sink, t.colorFormats[i], t.colorLoadOp[i], t.colorStoreOp[i]);
    }

    // Serialize the depth-stencil toggle bit, and the parameters if applicable.
    Serialize(sink, t.hasDepthStencil);
    if (t.hasDepthStencil) {
        Serialize(sink, t.depthStencilFormat, t.depthLoadOp, t.depthStoreOp, t.stencilLoadOp,
                  t.stencilStoreOp, t.readOnlyDepthStencil);
    }
}

template <>
void serde::Serde<VkGraphicsPipelineCreateInfo>::SerializeImpl(
    serde::Sink* sink,
    const VkGraphicsPipelineCreateInfo& t) {
    // The pipeline layout and render pass are not serialized here because they are pointers to
    // backend objects. They need to be cross-referenced with the frontend objects and
    // serialized from there. The base pipeline information is also currently not Serializeed since
    // we do not use them in our backend implementation. If we decide to use them later on, they
    // also need to be cross-referenced from the frontend.
    Serialize(sink, t.flags, Iterable(t.pStages, t.stageCount), t.pVertexInputState,
              t.pInputAssemblyState, t.pTessellationState, t.pViewportState, t.pRasterizationState,
              t.pMultisampleState, t.pDepthStencilState, t.pColorBlendState, t.pDynamicState,
              t.subpass);
    SerializePnext(sink, &t);
}

}  // namespace dawn::native
