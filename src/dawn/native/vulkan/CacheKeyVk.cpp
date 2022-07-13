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
        CacheKeyRecorder(sink).Record(found);
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
void CacheKeySerializer<VkDescriptorSetLayoutBinding>::Serialize(
    serde::Sink* sink,
    const VkDescriptorSetLayoutBinding& t) {
    CacheKeyRecorder(sink).Record(t.binding, t.descriptorType, t.descriptorCount, t.stageFlags);
}

template <>
void CacheKeySerializer<VkDescriptorSetLayoutCreateInfo>::Serialize(
    serde::Sink* sink,
    const VkDescriptorSetLayoutCreateInfo& t) {
    CacheKeyRecorder(sink).Record(t.flags).RecordIterable(t.pBindings, t.bindingCount);
    SerializePnext(sink, &t);
}

template <>
void CacheKeySerializer<VkPushConstantRange>::Serialize(serde::Sink* sink,
                                                        const VkPushConstantRange& t) {
    CacheKeyRecorder(sink).Record(t.stageFlags, t.offset, t.size);
}

template <>
void CacheKeySerializer<VkPipelineLayoutCreateInfo>::Serialize(
    serde::Sink* sink,
    const VkPipelineLayoutCreateInfo& t) {
    // The set layouts are not serialized here because they are pointers to backend objects.
    // They need to be cross-referenced with the frontend objects and serialized from there.
    CacheKeyRecorder(sink).Record(t.flags).RecordIterable(t.pPushConstantRanges,
                                                          t.pushConstantRangeCount);
    SerializePnext(sink, &t);
}

template <>
void CacheKeySerializer<VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT>::Serialize(
    serde::Sink* sink,
    const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT& t) {
    CacheKeyRecorder(sink).Record(t.requiredSubgroupSize);
}

template <>
void CacheKeySerializer<VkSpecializationMapEntry>::Serialize(serde::Sink* sink,
                                                             const VkSpecializationMapEntry& t) {
    CacheKeyRecorder(sink).Record(t.constantID, t.offset, t.size);
}

template <>
void CacheKeySerializer<VkSpecializationInfo>::Serialize(serde::Sink* sink,
                                                         const VkSpecializationInfo& t) {
    CacheKeyRecorder(sink)
        .RecordIterable(t.pMapEntries, t.mapEntryCount)
        .RecordIterable(static_cast<const uint8_t*>(t.pData), t.dataSize);
}

template <>
void CacheKeySerializer<VkPipelineShaderStageCreateInfo>::Serialize(
    serde::Sink* sink,
    const VkPipelineShaderStageCreateInfo& t) {
    // The shader module is not serialized here because it is a pointer to a backend object.
    CacheKeyRecorder(sink)
        .Record(t.flags, t.stage)
        .RecordIterable(t.pName, strlen(t.pName))
        .Record(t.pSpecializationInfo);
    SerializePnext<VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT>(sink, &t);
}

template <>
void CacheKeySerializer<VkComputePipelineCreateInfo>::Serialize(
    serde::Sink* sink,
    const VkComputePipelineCreateInfo& t) {
    // The pipeline layout is not serialized here because it is a pointer to a backend object.
    // It needs to be cross-referenced with the frontend objects and serialized from there. The
    // base pipeline information is also currently not Serializeed since we do not use them in our
    // backend implementation. If we decide to use them later on, they also need to be
    // cross-referenced from the frontend.
    CacheKeyRecorder(sink).Record(t.flags, t.stage);
}

template <>
void CacheKeySerializer<VkVertexInputBindingDescription>::Serialize(
    serde::Sink* sink,
    const VkVertexInputBindingDescription& t) {
    CacheKeyRecorder(sink).Record(t.binding, t.stride, t.inputRate);
}

template <>
void CacheKeySerializer<VkVertexInputAttributeDescription>::Serialize(
    serde::Sink* sink,
    const VkVertexInputAttributeDescription& t) {
    CacheKeyRecorder(sink).Record(t.location, t.binding, t.format, t.offset);
}

template <>
void CacheKeySerializer<VkPipelineVertexInputStateCreateInfo>::Serialize(
    serde::Sink* sink,
    const VkPipelineVertexInputStateCreateInfo& t) {
    CacheKeyRecorder(sink)
        .Record(t.flags)
        .RecordIterable(t.pVertexBindingDescriptions, t.vertexBindingDescriptionCount)
        .RecordIterable(t.pVertexAttributeDescriptions, t.vertexAttributeDescriptionCount);
    SerializePnext(sink, &t);
}

template <>
void CacheKeySerializer<VkPipelineInputAssemblyStateCreateInfo>::Serialize(
    serde::Sink* sink,
    const VkPipelineInputAssemblyStateCreateInfo& t) {
    CacheKeyRecorder(sink).Record(t.flags, t.topology, t.primitiveRestartEnable);
    SerializePnext(sink, &t);
}

template <>
void CacheKeySerializer<VkPipelineTessellationStateCreateInfo>::Serialize(
    serde::Sink* sink,
    const VkPipelineTessellationStateCreateInfo& t) {
    CacheKeyRecorder(sink).Record(t.flags, t.patchControlPoints);
    SerializePnext(sink, &t);
}

template <>
void CacheKeySerializer<VkViewport>::Serialize(serde::Sink* sink, const VkViewport& t) {
    CacheKeyRecorder(sink).Record(t.x, t.y, t.width, t.height, t.minDepth, t.maxDepth);
}

template <>
void CacheKeySerializer<VkOffset2D>::Serialize(serde::Sink* sink, const VkOffset2D& t) {
    CacheKeyRecorder(sink).Record(t.x, t.y);
}

template <>
void CacheKeySerializer<VkExtent2D>::Serialize(serde::Sink* sink, const VkExtent2D& t) {
    CacheKeyRecorder(sink).Record(t.width, t.height);
}

template <>
void CacheKeySerializer<VkRect2D>::Serialize(serde::Sink* sink, const VkRect2D& t) {
    CacheKeyRecorder(sink).Record(t.offset, t.extent);
}

template <>
void CacheKeySerializer<VkPipelineViewportStateCreateInfo>::Serialize(
    serde::Sink* sink,
    const VkPipelineViewportStateCreateInfo& t) {
    CacheKeyRecorder(sink)
        .Record(t.flags)
        .RecordIterable(t.pViewports, t.viewportCount)
        .RecordIterable(t.pScissors, t.scissorCount);
    SerializePnext(sink, &t);
}

template <>
void CacheKeySerializer<VkPipelineRasterizationStateCreateInfo>::Serialize(
    serde::Sink* sink,
    const VkPipelineRasterizationStateCreateInfo& t) {
    CacheKeyRecorder(sink).Record(t.flags, t.depthClampEnable, t.rasterizerDiscardEnable,
                                  t.polygonMode, t.cullMode, t.frontFace, t.depthBiasEnable,
                                  t.depthBiasConstantFactor, t.depthBiasClamp,
                                  t.depthBiasSlopeFactor, t.lineWidth);
    SerializePnext(sink, &t);
}

template <>
void CacheKeySerializer<VkPipelineMultisampleStateCreateInfo>::Serialize(
    serde::Sink* sink,
    const VkPipelineMultisampleStateCreateInfo& t) {
    CacheKeyRecorder(sink).Record(t.flags, t.rasterizationSamples, t.sampleShadingEnable,
                                  t.minSampleShading, t.pSampleMask, t.alphaToCoverageEnable,
                                  t.alphaToOneEnable);
    SerializePnext(sink, &t);
}

template <>
void CacheKeySerializer<VkStencilOpState>::Serialize(serde::Sink* sink, const VkStencilOpState& t) {
    CacheKeyRecorder(sink).Record(t.failOp, t.passOp, t.depthFailOp, t.compareOp, t.compareMask,
                                  t.writeMask, t.reference);
}

template <>
void CacheKeySerializer<VkPipelineDepthStencilStateCreateInfo>::Serialize(
    serde::Sink* sink,
    const VkPipelineDepthStencilStateCreateInfo& t) {
    CacheKeyRecorder(sink).Record(t.flags, t.depthTestEnable, t.depthWriteEnable, t.depthCompareOp,
                                  t.depthBoundsTestEnable, t.stencilTestEnable, t.front, t.back,
                                  t.minDepthBounds, t.maxDepthBounds);
    SerializePnext(sink, &t);
}

template <>
void CacheKeySerializer<VkPipelineColorBlendAttachmentState>::Serialize(
    serde::Sink* sink,
    const VkPipelineColorBlendAttachmentState& t) {
    CacheKeyRecorder(sink).Record(t.blendEnable, t.srcColorBlendFactor, t.dstColorBlendFactor,
                                  t.colorBlendOp, t.srcAlphaBlendFactor, t.dstAlphaBlendFactor,
                                  t.alphaBlendOp, t.colorWriteMask);
}

template <>
void CacheKeySerializer<VkPipelineColorBlendStateCreateInfo>::Serialize(
    serde::Sink* sink,
    const VkPipelineColorBlendStateCreateInfo& t) {
    CacheKeyRecorder(sink)
        .Record(t.flags, t.logicOpEnable, t.logicOp)
        .RecordIterable(t.pAttachments, t.attachmentCount)
        .Record(t.blendConstants);
    SerializePnext(sink, &t);
}

template <>
void CacheKeySerializer<VkPipelineDynamicStateCreateInfo>::Serialize(
    serde::Sink* sink,
    const VkPipelineDynamicStateCreateInfo& t) {
    CacheKeyRecorder(sink).Record(t.flags).RecordIterable(t.pDynamicStates, t.dynamicStateCount);
    SerializePnext(sink, &t);
}

template <>
void CacheKeySerializer<vulkan::RenderPassCacheQuery>::Serialize(
    serde::Sink* sink,
    const vulkan::RenderPassCacheQuery& t) {
    CacheKeyRecorder(sink).Record(t.colorMask.to_ulong(), t.resolveTargetMask.to_ulong(),
                                  t.sampleCount);

    // Manually iterate the color attachment indices and their corresponding format/load/store
    // ops because the data is sparse and may be uninitialized. Since we Serialize the colorMask
    // member above, Serializeing sparse data should be fine here.
    for (ColorAttachmentIndex i : IterateBitSet(t.colorMask)) {
        CacheKeyRecorder(sink).Record(t.colorFormats[i], t.colorLoadOp[i], t.colorStoreOp[i]);
    }

    // Serialize the depth-stencil toggle bit, and the parameters if applicable.
    CacheKeyRecorder(sink).Record(t.hasDepthStencil);
    if (t.hasDepthStencil) {
        CacheKeyRecorder(sink).Record(t.depthStencilFormat, t.depthLoadOp, t.depthStoreOp,
                                      t.stencilLoadOp, t.stencilStoreOp, t.readOnlyDepthStencil);
    }
}

template <>
void CacheKeySerializer<VkGraphicsPipelineCreateInfo>::Serialize(
    serde::Sink* sink,
    const VkGraphicsPipelineCreateInfo& t) {
    // The pipeline layout and render pass are not serialized here because they are pointers to
    // backend objects. They need to be cross-referenced with the frontend objects and
    // serialized from there. The base pipeline information is also currently not Serializeed since
    // we do not use them in our backend implementation. If we decide to use them later on, they
    // also need to be cross-referenced from the frontend.
    CacheKeyRecorder(sink)
        .Record(t.flags)
        .RecordIterable(t.pStages, t.stageCount)
        .Record(t.pVertexInputState, t.pInputAssemblyState, t.pTessellationState, t.pViewportState,
                t.pRasterizationState, t.pMultisampleState, t.pDepthStencilState,
                t.pColorBlendState, t.pDynamicState, t.subpass);
    SerializePnext(sink, &t);
}

}  // namespace dawn::native
