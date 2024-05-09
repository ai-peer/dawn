// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "dawn/native/vulkan/ResolveTextureLoadingUtilsVk.h"

#include "dawn/native/Commands.h"
#include "dawn/native/vulkan/DescriptorSetAllocator.h"
#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/native/vulkan/FencedDeleter.h"
#include "dawn/native/vulkan/RenderPassCache.h"
#include "dawn/native/vulkan/TextureVk.h"
#include "dawn/native/vulkan/UtilsVulkan.h"
#include "dawn/native/vulkan/VulkanError.h"

#include <utility>

namespace dawn::native::vulkan {

namespace {
// TODO(dawn:1710): Auto-generate this
// #version 450
//
// const vec2 gFullScreenTriPositions[] = vec2[] (
//     vec2(-1.0, -1.0),
//     vec2( 3.0, -1.0),
//     vec2(-1.0,  3.0)
// );
//
// void main() {
//     gl_Position = vec4(gFullScreenTriPositions[gl_VertexIndex], 0.0, 1.0);
// }
static constexpr uint8_t gUnresolveVS[] = {
    3,   2,   35,  7,   0,   0,   1,   0,   11,  0,   8,   0,   40,  0,   0,   0,   0,   0,   0,
    0,   17,  0,   2,   0,   1,   0,   0,   0,   11,  0,   6,   0,   1,   0,   0,   0,   71,  76,
    83,  76,  46,  115, 116, 100, 46,  52,  53,  48,  0,   0,   0,   0,   14,  0,   3,   0,   0,
    0,   0,   0,   1,   0,   0,   0,   15,  0,   7,   0,   0,   0,   0,   0,   4,   0,   0,   0,
    109, 97,  105, 110, 0,   0,   0,   0,   13,  0,   0,   0,   26,  0,   0,   0,   3,   0,   3,
    0,   2,   0,   0,   0,   194, 1,   0,   0,   5,   0,   4,   0,   4,   0,   0,   0,   109, 97,
    105, 110, 0,   0,   0,   0,   5,   0,   6,   0,   11,  0,   0,   0,   103, 108, 95,  80,  101,
    114, 86,  101, 114, 116, 101, 120, 0,   0,   0,   0,   6,   0,   6,   0,   11,  0,   0,   0,
    0,   0,   0,   0,   103, 108, 95,  80,  111, 115, 105, 116, 105, 111, 110, 0,   6,   0,   7,
    0,   11,  0,   0,   0,   1,   0,   0,   0,   103, 108, 95,  80,  111, 105, 110, 116, 83,  105,
    122, 101, 0,   0,   0,   0,   6,   0,   7,   0,   11,  0,   0,   0,   2,   0,   0,   0,   103,
    108, 95,  67,  108, 105, 112, 68,  105, 115, 116, 97,  110, 99,  101, 0,   6,   0,   7,   0,
    11,  0,   0,   0,   3,   0,   0,   0,   103, 108, 95,  67,  117, 108, 108, 68,  105, 115, 116,
    97,  110, 99,  101, 0,   5,   0,   3,   0,   13,  0,   0,   0,   0,   0,   0,   0,   5,   0,
    6,   0,   26,  0,   0,   0,   103, 108, 95,  86,  101, 114, 116, 101, 120, 73,  110, 100, 101,
    120, 0,   0,   5,   0,   5,   0,   29,  0,   0,   0,   105, 110, 100, 101, 120, 97,  98,  108,
    101, 0,   0,   0,   72,  0,   5,   0,   11,  0,   0,   0,   0,   0,   0,   0,   11,  0,   0,
    0,   0,   0,   0,   0,   72,  0,   5,   0,   11,  0,   0,   0,   1,   0,   0,   0,   11,  0,
    0,   0,   1,   0,   0,   0,   72,  0,   5,   0,   11,  0,   0,   0,   2,   0,   0,   0,   11,
    0,   0,   0,   3,   0,   0,   0,   72,  0,   5,   0,   11,  0,   0,   0,   3,   0,   0,   0,
    11,  0,   0,   0,   4,   0,   0,   0,   71,  0,   3,   0,   11,  0,   0,   0,   2,   0,   0,
    0,   71,  0,   4,   0,   26,  0,   0,   0,   11,  0,   0,   0,   42,  0,   0,   0,   19,  0,
    2,   0,   2,   0,   0,   0,   33,  0,   3,   0,   3,   0,   0,   0,   2,   0,   0,   0,   22,
    0,   3,   0,   6,   0,   0,   0,   32,  0,   0,   0,   23,  0,   4,   0,   7,   0,   0,   0,
    6,   0,   0,   0,   4,   0,   0,   0,   21,  0,   4,   0,   8,   0,   0,   0,   32,  0,   0,
    0,   0,   0,   0,   0,   43,  0,   4,   0,   8,   0,   0,   0,   9,   0,   0,   0,   1,   0,
    0,   0,   28,  0,   4,   0,   10,  0,   0,   0,   6,   0,   0,   0,   9,   0,   0,   0,   30,
    0,   6,   0,   11,  0,   0,   0,   7,   0,   0,   0,   6,   0,   0,   0,   10,  0,   0,   0,
    10,  0,   0,   0,   32,  0,   4,   0,   12,  0,   0,   0,   3,   0,   0,   0,   11,  0,   0,
    0,   59,  0,   4,   0,   12,  0,   0,   0,   13,  0,   0,   0,   3,   0,   0,   0,   21,  0,
    4,   0,   14,  0,   0,   0,   32,  0,   0,   0,   1,   0,   0,   0,   43,  0,   4,   0,   14,
    0,   0,   0,   15,  0,   0,   0,   0,   0,   0,   0,   23,  0,   4,   0,   16,  0,   0,   0,
    6,   0,   0,   0,   2,   0,   0,   0,   43,  0,   4,   0,   8,   0,   0,   0,   17,  0,   0,
    0,   3,   0,   0,   0,   28,  0,   4,   0,   18,  0,   0,   0,   16,  0,   0,   0,   17,  0,
    0,   0,   43,  0,   4,   0,   6,   0,   0,   0,   19,  0,   0,   0,   0,   0,   128, 191, 44,
    0,   5,   0,   16,  0,   0,   0,   20,  0,   0,   0,   19,  0,   0,   0,   19,  0,   0,   0,
    43,  0,   4,   0,   6,   0,   0,   0,   21,  0,   0,   0,   0,   0,   64,  64,  44,  0,   5,
    0,   16,  0,   0,   0,   22,  0,   0,   0,   21,  0,   0,   0,   19,  0,   0,   0,   44,  0,
    5,   0,   16,  0,   0,   0,   23,  0,   0,   0,   19,  0,   0,   0,   21,  0,   0,   0,   44,
    0,   6,   0,   18,  0,   0,   0,   24,  0,   0,   0,   20,  0,   0,   0,   22,  0,   0,   0,
    23,  0,   0,   0,   32,  0,   4,   0,   25,  0,   0,   0,   1,   0,   0,   0,   14,  0,   0,
    0,   59,  0,   4,   0,   25,  0,   0,   0,   26,  0,   0,   0,   1,   0,   0,   0,   32,  0,
    4,   0,   28,  0,   0,   0,   7,   0,   0,   0,   18,  0,   0,   0,   32,  0,   4,   0,   30,
    0,   0,   0,   7,   0,   0,   0,   16,  0,   0,   0,   43,  0,   4,   0,   6,   0,   0,   0,
    33,  0,   0,   0,   0,   0,   0,   0,   43,  0,   4,   0,   6,   0,   0,   0,   34,  0,   0,
    0,   0,   0,   128, 63,  32,  0,   4,   0,   38,  0,   0,   0,   3,   0,   0,   0,   7,   0,
    0,   0,   54,  0,   5,   0,   2,   0,   0,   0,   4,   0,   0,   0,   0,   0,   0,   0,   3,
    0,   0,   0,   248, 0,   2,   0,   5,   0,   0,   0,   59,  0,   4,   0,   28,  0,   0,   0,
    29,  0,   0,   0,   7,   0,   0,   0,   61,  0,   4,   0,   14,  0,   0,   0,   27,  0,   0,
    0,   26,  0,   0,   0,   62,  0,   3,   0,   29,  0,   0,   0,   24,  0,   0,   0,   65,  0,
    5,   0,   30,  0,   0,   0,   31,  0,   0,   0,   29,  0,   0,   0,   27,  0,   0,   0,   61,
    0,   4,   0,   16,  0,   0,   0,   32,  0,   0,   0,   31,  0,   0,   0,   81,  0,   5,   0,
    6,   0,   0,   0,   35,  0,   0,   0,   32,  0,   0,   0,   0,   0,   0,   0,   81,  0,   5,
    0,   6,   0,   0,   0,   36,  0,   0,   0,   32,  0,   0,   0,   1,   0,   0,   0,   80,  0,
    7,   0,   7,   0,   0,   0,   37,  0,   0,   0,   35,  0,   0,   0,   36,  0,   0,   0,   33,
    0,   0,   0,   34,  0,   0,   0,   65,  0,   5,   0,   38,  0,   0,   0,   39,  0,   0,   0,
    13,  0,   0,   0,   15,  0,   0,   0,   62,  0,   3,   0,   39,  0,   0,   0,   37,  0,   0,
    0,   253, 0,   1,   0,   56,  0,   1,   0,
};
static_assert((std::size(gUnresolveVS) % 4) == 0, "gUnresolveVS must be multiples of 4 bytes");

// #version 450
//
// layout(set = 0, binding = 0, input_attachment_index=0) uniform subpassInput uResolveTexture;
//
// layout(location = 0) out vec4 oColor;
//
// void main() {
//     oColor = subpassLoad(uResolveTexture);
// }
static constexpr uint8_t gUnresolveFloatFS[] = {
    3,   2,  35,  7,   0,   0,   1,   0,   11,  0,   8,   0,   19,  0,   0,   0,  0,   0, 0, 0,
    17,  0,  2,   0,   1,   0,   0,   0,   17,  0,   2,   0,   40,  0,   0,   0,  11,  0, 6, 0,
    1,   0,  0,   0,   71,  76,  83,  76,  46,  115, 116, 100, 46,  52,  53,  48, 0,   0, 0, 0,
    14,  0,  3,   0,   0,   0,   0,   0,   1,   0,   0,   0,   15,  0,   6,   0,  4,   0, 0, 0,
    4,   0,  0,   0,   109, 97,  105, 110, 0,   0,   0,   0,   9,   0,   0,   0,  16,  0, 3, 0,
    4,   0,  0,   0,   7,   0,   0,   0,   3,   0,   3,   0,   2,   0,   0,   0,  194, 1, 0, 0,
    5,   0,  4,   0,   4,   0,   0,   0,   109, 97,  105, 110, 0,   0,   0,   0,  5,   0, 4, 0,
    9,   0,  0,   0,   111, 67,  111, 108, 111, 114, 0,   0,   5,   0,   6,   0,  12,  0, 0, 0,
    117, 82, 101, 115, 111, 108, 118, 101, 84,  101, 120, 116, 117, 114, 101, 0,  71,  0, 4, 0,
    9,   0,  0,   0,   30,  0,   0,   0,   0,   0,   0,   0,   71,  0,   4,   0,  12,  0, 0, 0,
    34,  0,  0,   0,   0,   0,   0,   0,   71,  0,   4,   0,   12,  0,   0,   0,  33,  0, 0, 0,
    0,   0,  0,   0,   71,  0,   4,   0,   12,  0,   0,   0,   43,  0,   0,   0,  0,   0, 0, 0,
    19,  0,  2,   0,   2,   0,   0,   0,   33,  0,   3,   0,   3,   0,   0,   0,  2,   0, 0, 0,
    22,  0,  3,   0,   6,   0,   0,   0,   32,  0,   0,   0,   23,  0,   4,   0,  7,   0, 0, 0,
    6,   0,  0,   0,   4,   0,   0,   0,   32,  0,   4,   0,   8,   0,   0,   0,  3,   0, 0, 0,
    7,   0,  0,   0,   59,  0,   4,   0,   8,   0,   0,   0,   9,   0,   0,   0,  3,   0, 0, 0,
    25,  0,  9,   0,   10,  0,   0,   0,   6,   0,   0,   0,   6,   0,   0,   0,  0,   0, 0, 0,
    0,   0,  0,   0,   0,   0,   0,   0,   2,   0,   0,   0,   0,   0,   0,   0,  32,  0, 4, 0,
    11,  0,  0,   0,   0,   0,   0,   0,   10,  0,   0,   0,   59,  0,   4,   0,  11,  0, 0, 0,
    12,  0,  0,   0,   0,   0,   0,   0,   21,  0,   4,   0,   14,  0,   0,   0,  32,  0, 0, 0,
    1,   0,  0,   0,   43,  0,   4,   0,   14,  0,   0,   0,   15,  0,   0,   0,  0,   0, 0, 0,
    23,  0,  4,   0,   16,  0,   0,   0,   14,  0,   0,   0,   2,   0,   0,   0,  44,  0, 5, 0,
    16,  0,  0,   0,   17,  0,   0,   0,   15,  0,   0,   0,   15,  0,   0,   0,  54,  0, 5, 0,
    2,   0,  0,   0,   4,   0,   0,   0,   0,   0,   0,   0,   3,   0,   0,   0,  248, 0, 2, 0,
    5,   0,  0,   0,   61,  0,   4,   0,   10,  0,   0,   0,   13,  0,   0,   0,  12,  0, 0, 0,
    98,  0,  5,   0,   7,   0,   0,   0,   18,  0,   0,   0,   13,  0,   0,   0,  17,  0, 0, 0,
    62,  0,  3,   0,   9,   0,   0,   0,   18,  0,   0,   0,   253, 0,   1,   0,  56,  0, 1, 0,
};
static_assert((std::size(gUnresolveFloatFS) % 4) == 0,
              "gUnresolveFloatFS must be multiples of 4 bytes");

constexpr ColorAttachmentIndex kZeroAttachmentIdx(static_cast<uint8_t>(0));

ResultOrError<VkDescriptorSetLayout> GetOrCreateLoadResolveTextureDescriptorSetLayout(
    Device* device) {
    InternalPipelineStore* store = device->GetInternalPipelineStoreVk();
    if (store->loadResolveTextureWithDrawDesctiptorSetLayout != VK_NULL_HANDLE) {
        return store->loadResolveTextureWithDrawDesctiptorSetLayout;
    }

    VkDescriptorSetLayoutBinding inputLayoutBinding{};
    inputLayoutBinding.binding = 0;
    inputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    inputLayoutBinding.descriptorCount = 1;
    inputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &inputLayoutBinding;

    DAWN_TRY(CheckVkSuccess(device->fn.CreateDescriptorSetLayout(device->GetVkDevice(), &layoutInfo,
                                                                 nullptr, &*descriptorSetLayout),
                            "CreateDescriptorSetLayout"));

    return store->loadResolveTextureWithDrawDesctiptorSetLayout = descriptorSetLayout;
}

ResultOrError<VkPipelineLayout> GetOrCreateLoadResolveTexturePipelineLayout(Device* device) {
    InternalPipelineStore* store = device->GetInternalPipelineStoreVk();
    if (store->loadResolveTextureWithDrawPipelineLayout != VK_NULL_HANDLE) {
        return store->loadResolveTextureWithDrawPipelineLayout;
    }

    VkDescriptorSetLayout descriptorSetLayout;
    DAWN_TRY_ASSIGN(descriptorSetLayout, GetOrCreateLoadResolveTextureDescriptorSetLayout(device));

    VkPipelineLayout pipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &*descriptorSetLayout;
    DAWN_TRY(
        CheckVkSuccess(device->fn.CreatePipelineLayout(device->GetVkDevice(), &pipelineLayoutInfo,
                                                       nullptr, &*pipelineLayout),
                       "CreatePipelineLayout"));

    return store->loadResolveTextureWithDrawPipelineLayout = pipelineLayout;
}

Ref<DescriptorSetAllocator> GetOrCreateLoadResolveTextureDescriptorSetAllocator(Device* device) {
    InternalPipelineStore* store = device->GetInternalPipelineStoreVk();
    if (store->loadResolveTextureWithDrawDesctiptorSetAllocator) {
        return store->loadResolveTextureWithDrawDesctiptorSetAllocator;
    }

    absl::flat_hash_map<VkDescriptorType, uint32_t> descriptorCountPerType;
    descriptorCountPerType[VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT] = 1;

    return store->loadResolveTextureWithDrawDesctiptorSetAllocator =
               DescriptorSetAllocator::Create(device, std::move(descriptorCountPerType));
}

VkPipelineDepthStencilStateCreateInfo CreateDepthStencilCreateInfo() {
    VkPipelineDepthStencilStateCreateInfo depthStencilState;
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.pNext = nullptr;
    depthStencilState.flags = 0;

    depthStencilState.depthTestEnable = VK_FALSE;
    depthStencilState.depthWriteEnable = VK_FALSE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilState.depthBoundsTestEnable = false;
    depthStencilState.minDepthBounds = 0.0f;
    depthStencilState.maxDepthBounds = 1.0f;

    depthStencilState.stencilTestEnable = VK_FALSE;

    depthStencilState.front.failOp = VK_STENCIL_OP_KEEP;
    depthStencilState.front.passOp = VK_STENCIL_OP_KEEP;
    depthStencilState.front.depthFailOp = VK_STENCIL_OP_KEEP;
    depthStencilState.front.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilState.front.compareMask = 0xffffffff;
    depthStencilState.front.writeMask = 0xffffffff;
    depthStencilState.front.reference = 0;

    depthStencilState.back = depthStencilState.front;

    return depthStencilState;
}

ResultOrError<VkPipeline> GetOrCreateLoadResolveTexturePipeline(
    Device* device,
    const Format& colorInternalFormat,
    wgpu::TextureFormat depthStencilFormat,
    uint32_t sampleCount) {
    InternalPipelineStore* store = device->GetInternalPipelineStoreVk();
    BlitColorToColorWithDrawPipelineKey pipelineKey;
    pipelineKey.colorFormat = colorInternalFormat.format;
    pipelineKey.depthStencilFormat = depthStencilFormat;
    pipelineKey.sampleCount = sampleCount;
    {
        auto it = store->loadResolveTextureWithDrawPipelines.find(pipelineKey);
        if (it != store->loadResolveTextureWithDrawPipelines.end()) {
            return it->second;
        }
    }

    const auto& formatAspectInfo = colorInternalFormat.GetAspectInfo(Aspect::Color);

    // vertex shader's source.
    VkShaderModuleCreateInfo shaderCreateInfo{};
    shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderCreateInfo.codeSize = std::size(gUnresolveVS);
    shaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(gUnresolveVS);

    VkShaderModule vsShaderModule;
    DAWN_TRY(CheckVkSuccess(device->fn.CreateShaderModule(device->GetVkDevice(), &shaderCreateInfo,
                                                          nullptr, &*vsShaderModule),
                            "CreateShaderModule"));
    device->GetFencedDeleter()->DeleteWhenUnused(vsShaderModule);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vsShaderModule;
    vertShaderStageInfo.pName = "main";

    // fragment shader's source will depend on color format type.
    switch (formatAspectInfo.baseType) {
        case TextureComponentType::Float:
            shaderCreateInfo.codeSize = std::size(gUnresolveFloatFS);
            shaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(gUnresolveFloatFS);
            break;
        default:
            // TODO(dawn:1710): blitting integer textures are not currently supported.
            DAWN_UNREACHABLE();
            break;
    }
    VkShaderModule fsShaderModule;
    DAWN_TRY(CheckVkSuccess(device->fn.CreateShaderModule(device->GetVkDevice(), &shaderCreateInfo,
                                                          nullptr, &*fsShaderModule),
                            "CreateShaderModule"));
    device->GetFencedDeleter()->DeleteWhenUnused(fsShaderModule);

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fsShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Dynamic states
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = std::size(dynamicStates);
    dynamicState.pDynamicStates = dynamicStates;

    // Vertex input (use none)
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // A placeholder viewport/scissor info. The validation layers force use to provide at least
    // one scissor and one viewport here, even if we choose to make them dynamic.
    VkViewport viewportDesc;
    viewportDesc.x = 0.0f;
    viewportDesc.y = 0.0f;
    viewportDesc.width = 1.0f;
    viewportDesc.height = 1.0f;
    viewportDesc.minDepth = 0.0f;
    viewportDesc.maxDepth = 1.0f;
    VkRect2D scissorRect;
    scissorRect.offset.x = 0;
    scissorRect.offset.y = 0;
    scissorRect.extent.width = 1;
    scissorRect.extent.height = 1;
    VkPipelineViewportStateCreateInfo viewport;
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.pNext = nullptr;
    viewport.flags = 0;
    viewport.viewportCount = 1;
    viewport.pViewports = &viewportDesc;
    viewport.scissorCount = 1;
    viewport.pScissors = &scissorRect;

    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.pNext = nullptr;
    rasterization.depthClampEnable = VK_FALSE;
    rasterization.rasterizerDiscardEnable = VK_FALSE;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.depthBiasEnable = VK_FALSE;
    rasterization.depthBiasConstantFactor = 0.0f;
    rasterization.depthBiasClamp = 0.0f;
    rasterization.depthBiasSlopeFactor = 0.0f;
    rasterization.lineWidth = 1.0f;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask = 0;

    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable = VK_FALSE;
    colorBlend.logicOp = VK_LOGIC_OP_CLEAR;
    // TODO(dawn:1710): Only one color attachment is allowed for now.
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachment;
    // The blend constant is always dynamic so we fill in a placeholder value
    colorBlend.blendConstants[0] = 0.0f;
    colorBlend.blendConstants[1] = 0.0f;
    colorBlend.blendConstants[2] = 0.0f;
    colorBlend.blendConstants[3] = 0.0f;

    // Multisample state.
    DAWN_ASSERT(sampleCount > 1);
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VulkanSampleCount(sampleCount);
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.minSampleShading = 0.0f;
    multisample.pSampleMask = nullptr;
    multisample.alphaToCoverageEnable = VK_FALSE;
    multisample.alphaToOneEnable = VK_FALSE;

    // Depth stencil state
    VkPipelineDepthStencilStateCreateInfo depthStencilState = CreateDepthStencilCreateInfo();

    // Get compatible render pass
    VkRenderPass renderPass = VK_NULL_HANDLE;
    {
        RenderPassCacheQuery query;

        // TODO(dawn:1710): only one attachment is supported for now.
        query.SetColor(kZeroAttachmentIdx, colorInternalFormat.format,
                       wgpu::LoadOp::ExpandResolveTexture, wgpu::StoreOp::Store, true);

        if (depthStencilFormat != wgpu::TextureFormat::Undefined) {
            query.SetDepthStencil(depthStencilFormat, wgpu::LoadOp::Load, wgpu::StoreOp::Store,
                                  false, wgpu::LoadOp::Load, wgpu::StoreOp::Store, false);
        }

        query.SetSampleCount(sampleCount);

        DAWN_TRY_ASSIGN(renderPass, device->GetRenderPassCache()->GetRenderPass(query));
    }

    // Layout
    VkPipelineLayout layout;
    DAWN_TRY_ASSIGN(layout, GetOrCreateLoadResolveTexturePipelineLayout(device));

    // The create info chains in a bunch of things created on the stack here or inside state
    // objects.
    VkGraphicsPipelineCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.stageCount = std::size(shaderStages);
    createInfo.pStages = shaderStages;
    createInfo.pVertexInputState = &vertexInputInfo;
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pTessellationState = nullptr;
    createInfo.pViewportState = &viewport;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pMultisampleState = &multisample;
    createInfo.pDepthStencilState = &depthStencilState;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pDynamicState = &dynamicState;
    createInfo.layout = layout;
    createInfo.renderPass = renderPass;
    createInfo.subpass = 0;
    createInfo.basePipelineHandle = VkPipeline{};
    createInfo.basePipelineIndex = -1;

    // TODO(dawn:1710): pipeline cache?
    VkPipeline pipeline;
    DAWN_TRY(CheckVkSuccess(
        device->fn.CreateGraphicsPipelines(device->GetVkDevice(), (VkPipelineCache)VK_NULL_HANDLE,
                                           1, &createInfo, nullptr, &*pipeline),
        "CreateGraphicsPipelines"));

    store->loadResolveTextureWithDrawPipelines.emplace(pipelineKey, pipeline);
    return pipeline;
}
}  // namespace

MaybeError ExpandResolveTextureWithDrawInSubpass(Device* device,
                                                 VkCommandBuffer commandBuffer,
                                                 const BeginRenderPassCmd* renderPass) {
    DAWN_ASSERT(device->IsLockedByCurrentThreadIfNeeded());
    DAWN_ASSERT(renderPass->attachmentState->HasExpandResolveLoadOp());

    // TODO(dawn:1710): support multiple attachments.
    DAWN_ASSERT(renderPass->attachmentState->GetColorAttachmentsMask().count() == 1);

    TextureViewBase* src = renderPass->colorAttachments[kZeroAttachmentIdx].resolveTarget.Get();
    TextureViewBase* dst = renderPass->colorAttachments[kZeroAttachmentIdx].view.Get();
    TextureBase* dstTexture = dst->GetTexture();

    wgpu::TextureFormat depthStencilFormat = wgpu::TextureFormat::Undefined;
    if (renderPass->attachmentState->HasDepthStencilAttachment()) {
        depthStencilFormat = renderPass->attachmentState->GetDepthStencilFormat();
    }

    VkPipeline pipeline;
    DAWN_TRY_ASSIGN(pipeline, GetOrCreateLoadResolveTexturePipeline(
                                  device, src->GetFormat(), depthStencilFormat,
                                  /*sampleCount=*/dstTexture->GetSampleCount()));

    // Descriptor set
    VkDescriptorSetLayout descSetLayout;
    DAWN_TRY_ASSIGN(descSetLayout, GetOrCreateLoadResolveTextureDescriptorSetLayout(device));
    Ref<DescriptorSetAllocator> descSetAllocator =
        GetOrCreateLoadResolveTextureDescriptorSetAllocator(device);
    DescriptorSetAllocation descSet;
    DAWN_TRY_ASSIGN(descSet, descSetAllocator->Allocate(descSetLayout));

    VkDescriptorImageInfo inputAttachmentInfo;
    inputAttachmentInfo.sampler = (VkSampler)VK_NULL_HANDLE;
    inputAttachmentInfo.imageView = static_cast<TextureView*>(src)->GetHandle();
    inputAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet descSetWrite{};
    descSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descSetWrite.dstSet = descSet.set;
    descSetWrite.dstBinding = 0;
    descSetWrite.dstArrayElement = 0;
    descSetWrite.descriptorCount = 1;
    descSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    descSetWrite.pImageInfo = &inputAttachmentInfo;

    device->fn.UpdateDescriptorSets(device->GetVkDevice(), 1, &descSetWrite, 0, nullptr);

    // Draw to perform the blit.
    auto size3D = dst->GetSingleSubresourceVirtualSize();
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(size3D.width);
    viewport.height = static_cast<float>(size3D.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    device->fn.CmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent.width = size3D.width;
    scissor.extent.height = size3D.height;
    device->fn.CmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkPipelineLayout pipelineLayout;
    DAWN_TRY_ASSIGN(pipelineLayout, GetOrCreateLoadResolveTexturePipelineLayout(device));
    device->fn.CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    device->fn.CmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                                     0, 1, &*descSet.set, 0, nullptr);
    device->fn.CmdDraw(commandBuffer, 3, 1, 0, 0);

    // Schedule deletion
    descSetAllocator->Deallocate(&descSet);

    return {};
}
}  // namespace dawn::native::vulkan
