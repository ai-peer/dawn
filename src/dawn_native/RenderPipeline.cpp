// Copyright 2017 The Dawn Authors
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

#include "dawn_native/RenderPipeline.h"

#include "common/BitSetIterator.h"
#include "common/VertexFormatUtils.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"
#include "dawn_native/ObjectContentHasher.h"
#include "dawn_native/ValidationUtils_autogen.h"

#include <cmath>

namespace dawn_native {
    // Helper functions
    namespace {

        MaybeError ValidateVertexAttributeDescriptor(
            DeviceBase* device,
            const VertexAttributeDescriptor* attribute,
            uint64_t vertexBufferStride,
            std::bitset<kMaxVertexAttributes>* attributesSetMask) {
            DAWN_TRY(ValidateVertexFormat(attribute->format));

            if (dawn::IsDeprecatedVertexFormat(attribute->format)) {
                device->EmitDeprecationWarning(
                    "Vertex formats have changed and the old types will be removed soon.");
            }

            if (attribute->shaderLocation >= kMaxVertexAttributes) {
                return DAWN_VALIDATION_ERROR("Setting attribute out of bounds");
            }

            // No underflow is possible because the max vertex format size is smaller than
            // kMaxVertexBufferStride.
            ASSERT(kMaxVertexBufferStride >= dawn::VertexFormatSize(attribute->format));
            if (attribute->offset >
                kMaxVertexBufferStride - dawn::VertexFormatSize(attribute->format)) {
                return DAWN_VALIDATION_ERROR("Setting attribute offset out of bounds");
            }

            // No overflow is possible because the offset is already validated to be less
            // than kMaxVertexBufferStride.
            ASSERT(attribute->offset < kMaxVertexBufferStride);
            if (vertexBufferStride > 0 &&
                attribute->offset + dawn::VertexFormatSize(attribute->format) >
                    vertexBufferStride) {
                return DAWN_VALIDATION_ERROR("Setting attribute offset out of bounds");
            }

            if (attribute->offset % dawn::VertexFormatComponentSize(attribute->format) != 0) {
                return DAWN_VALIDATION_ERROR(
                    "Attribute offset needs to be a multiple of the size format's components");
            }

            if ((*attributesSetMask)[attribute->shaderLocation]) {
                return DAWN_VALIDATION_ERROR("Setting already set attribute");
            }

            attributesSetMask->set(attribute->shaderLocation);
            return {};
        }

        MaybeError ValidateVertexBufferLayoutDescriptor(
            DeviceBase* device,
            const VertexBufferLayoutDescriptor* buffer,
            std::bitset<kMaxVertexAttributes>* attributesSetMask) {
            DAWN_TRY(ValidateInputStepMode(buffer->stepMode));
            if (buffer->arrayStride > kMaxVertexBufferStride) {
                return DAWN_VALIDATION_ERROR("Setting arrayStride out of bounds");
            }

            if (buffer->arrayStride % 4 != 0) {
                return DAWN_VALIDATION_ERROR(
                    "arrayStride of Vertex buffer needs to be a multiple of 4 bytes");
            }

            for (uint32_t i = 0; i < buffer->attributeCount; ++i) {
                DAWN_TRY(ValidateVertexAttributeDescriptor(device, &buffer->attributes[i],
                                                           buffer->arrayStride, attributesSetMask));
            }

            return {};
        }

        MaybeError ValidateVertexStateDescriptor(
            DeviceBase* device,
            const VertexStateDescriptor* descriptor,
            wgpu::PrimitiveTopology primitiveTopology,
            std::bitset<kMaxVertexAttributes>* attributesSetMask) {
            if (descriptor->nextInChain != nullptr) {
                return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
            }
            DAWN_TRY(ValidateIndexFormat(descriptor->indexFormat));

            // Pipeline descriptors must have indexFormat != undefined IFF they are using strip
            // topologies.
            if (IsStripPrimitiveTopology(primitiveTopology)) {
                if (descriptor->indexFormat == wgpu::IndexFormat::Undefined) {
                    return DAWN_VALIDATION_ERROR(
                        "indexFormat must not be undefined when using strip primitive topologies");
                }
            } else if (descriptor->indexFormat != wgpu::IndexFormat::Undefined) {
                return DAWN_VALIDATION_ERROR(
                    "indexFormat must be undefined when using non-strip primitive topologies");
            }

            if (descriptor->vertexBufferCount > kMaxVertexBuffers) {
                return DAWN_VALIDATION_ERROR("Vertex buffer count exceeds maximum");
            }

            uint32_t totalAttributesNum = 0;
            for (uint32_t i = 0; i < descriptor->vertexBufferCount; ++i) {
                DAWN_TRY(ValidateVertexBufferLayoutDescriptor(device, &descriptor->vertexBuffers[i],
                                                              attributesSetMask));
                totalAttributesNum += descriptor->vertexBuffers[i].attributeCount;
            }

            // Every vertex attribute has a member called shaderLocation, and there are some
            // requirements for shaderLocation: 1) >=0, 2) values are different across different
            // attributes, 3) can't exceed kMaxVertexAttributes. So it can ensure that total
            // attribute number never exceed kMaxVertexAttributes.
            ASSERT(totalAttributesNum <= kMaxVertexAttributes);

            return {};
        }

        MaybeError ValidateRasterizationStateDescriptor(
            const RasterizationStateDescriptor* descriptor) {
            if (descriptor->nextInChain != nullptr) {
                return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
            }

            DAWN_TRY(ValidateFrontFace(descriptor->frontFace));
            DAWN_TRY(ValidateCullMode(descriptor->cullMode));

            if (std::isnan(descriptor->depthBiasSlopeScale) ||
                std::isnan(descriptor->depthBiasClamp)) {
                return DAWN_VALIDATION_ERROR("Depth bias parameters must not be NaN.");
            }

            return {};
        }

        MaybeError ValidateColorStateDescriptor(const DeviceBase* device,
                                                const ColorStateDescriptor& descriptor,
                                                bool fragmentWritten,
                                                wgpu::TextureComponentType fragmentOutputBaseType) {
            if (descriptor.nextInChain != nullptr) {
                return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
            }
            DAWN_TRY(ValidateBlendOperation(descriptor.alphaBlend.operation));
            DAWN_TRY(ValidateBlendFactor(descriptor.alphaBlend.srcFactor));
            DAWN_TRY(ValidateBlendFactor(descriptor.alphaBlend.dstFactor));
            DAWN_TRY(ValidateBlendOperation(descriptor.colorBlend.operation));
            DAWN_TRY(ValidateBlendFactor(descriptor.colorBlend.srcFactor));
            DAWN_TRY(ValidateBlendFactor(descriptor.colorBlend.dstFactor));
            DAWN_TRY(ValidateColorWriteMask(descriptor.writeMask));

            const Format* format;
            DAWN_TRY_ASSIGN(format, device->GetInternalFormat(descriptor.format));
            if (!format->IsColor() || !format->isRenderable) {
                return DAWN_VALIDATION_ERROR("Color format must be color renderable");
            }
            if (fragmentWritten &&
                fragmentOutputBaseType != format->GetAspectInfo(Aspect::Color).baseType) {
                return DAWN_VALIDATION_ERROR(
                    "Color format must match the fragment stage output type");
            }

            return {};
        }

        MaybeError ValidateDepthStencilStateDescriptor(
            const DeviceBase* device,
            const DepthStencilStateDescriptor* descriptor) {
            const ChainedStruct* chained = descriptor->nextInChain;
            if (chained != nullptr) {
                if (chained->sType != wgpu::SType::DepthStencilDepthClampingState) {
                    return DAWN_VALIDATION_ERROR("Unsupported sType");
                }
                if (!device->IsExtensionEnabled(Extension::DepthClamping)) {
                    return DAWN_VALIDATION_ERROR("The depth clamping feature is not supported");
                }
            }
            DAWN_TRY(ValidateCompareFunction(descriptor->depthCompare));
            DAWN_TRY(ValidateCompareFunction(descriptor->stencilFront.compare));
            DAWN_TRY(ValidateStencilOperation(descriptor->stencilFront.failOp));
            DAWN_TRY(ValidateStencilOperation(descriptor->stencilFront.depthFailOp));
            DAWN_TRY(ValidateStencilOperation(descriptor->stencilFront.passOp));
            DAWN_TRY(ValidateCompareFunction(descriptor->stencilBack.compare));
            DAWN_TRY(ValidateStencilOperation(descriptor->stencilBack.failOp));
            DAWN_TRY(ValidateStencilOperation(descriptor->stencilBack.depthFailOp));
            DAWN_TRY(ValidateStencilOperation(descriptor->stencilBack.passOp));

            const Format* format;
            DAWN_TRY_ASSIGN(format, device->GetInternalFormat(descriptor->format));
            if (!format->HasDepthOrStencil() || !format->isRenderable) {
                return DAWN_VALIDATION_ERROR(
                    "Depth stencil format must be depth-stencil renderable");
            }

            return {};
        }

    }  // anonymous namespace

    // Helper functions
    size_t IndexFormatSize(wgpu::IndexFormat format) {
        switch (format) {
            case wgpu::IndexFormat::Uint16:
                return sizeof(uint16_t);
            case wgpu::IndexFormat::Uint32:
                return sizeof(uint32_t);
            case wgpu::IndexFormat::Undefined:
                UNREACHABLE();
        }
    }

    bool IsStripPrimitiveTopology(wgpu::PrimitiveTopology primitiveTopology) {
        return primitiveTopology == wgpu::PrimitiveTopology::LineStrip ||
               primitiveTopology == wgpu::PrimitiveTopology::TriangleStrip;
    }

    MaybeError ValidateRenderPipelineDescriptor(DeviceBase* device,
                                                const RenderPipelineDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }

        if (descriptor->layout != nullptr) {
            DAWN_TRY(device->ValidateObject(descriptor->layout));
        }

        // TODO(crbug.com/dawn/136): Support vertex-only pipelines.
        if (descriptor->fragmentStage == nullptr) {
            return DAWN_VALIDATION_ERROR("Null fragment stage is not supported (yet)");
        }

        DAWN_TRY(ValidatePrimitiveTopology(descriptor->primitiveTopology));

        std::bitset<kMaxVertexAttributes> attributesSetMask;
        if (descriptor->vertexState) {
            DAWN_TRY(ValidateVertexStateDescriptor(device,
                descriptor->vertexState, descriptor->primitiveTopology, &attributesSetMask));
        }

        DAWN_TRY(ValidateProgrammableStageDescriptor(
            device, &descriptor->vertexStage, descriptor->layout, SingleShaderStage::Vertex));
        DAWN_TRY(ValidateProgrammableStageDescriptor(
            device, descriptor->fragmentStage, descriptor->layout, SingleShaderStage::Fragment));

        if (descriptor->rasterizationState) {
            DAWN_TRY(ValidateRasterizationStateDescriptor(descriptor->rasterizationState));
        }

        const EntryPointMetadata& vertexMetadata =
            descriptor->vertexStage.module->GetEntryPoint(descriptor->vertexStage.entryPoint);
        if (!IsSubset(vertexMetadata.usedVertexAttributes, attributesSetMask)) {
            return DAWN_VALIDATION_ERROR(
                "Pipeline vertex stage uses vertex buffers not in the vertex state");
        }

        if (!IsValidSampleCount(descriptor->sampleCount)) {
            return DAWN_VALIDATION_ERROR("Sample count is not supported");
        }

        if (descriptor->colorStateCount > kMaxColorAttachments) {
            return DAWN_VALIDATION_ERROR("Color States number exceeds maximum");
        }

        if (descriptor->colorStateCount == 0 && !descriptor->depthStencilState) {
            return DAWN_VALIDATION_ERROR(
                "Should have at least one colorState or a depthStencilState");
        }

        ASSERT(descriptor->fragmentStage != nullptr);
        const EntryPointMetadata& fragmentMetadata =
            descriptor->fragmentStage->module->GetEntryPoint(descriptor->fragmentStage->entryPoint);
        for (ColorAttachmentIndex i(uint8_t(0));
             i < ColorAttachmentIndex(static_cast<uint8_t>(descriptor->colorStateCount)); ++i) {
            DAWN_TRY(ValidateColorStateDescriptor(
                device, descriptor->colorStates[static_cast<uint8_t>(i)],
                fragmentMetadata.fragmentOutputsWritten[i],
                fragmentMetadata.fragmentOutputFormatBaseTypes[i]));
        }

        if (descriptor->depthStencilState) {
            DAWN_TRY(ValidateDepthStencilStateDescriptor(device, descriptor->depthStencilState));
        }

        if (descriptor->alphaToCoverageEnabled && descriptor->sampleCount <= 1) {
            return DAWN_VALIDATION_ERROR("Enabling alphaToCoverage requires sampleCount > 1");
        }

        return {};
    }

    bool StencilTestEnabled(const DepthStencilState* mDepthStencil) {
        return mDepthStencil->stencilBack.compare != wgpu::CompareFunction::Always ||
               mDepthStencil->stencilBack.failOp != wgpu::StencilOperation::Keep ||
               mDepthStencil->stencilBack.depthFailOp != wgpu::StencilOperation::Keep ||
               mDepthStencil->stencilBack.passOp != wgpu::StencilOperation::Keep ||
               mDepthStencil->stencilFront.compare != wgpu::CompareFunction::Always ||
               mDepthStencil->stencilFront.failOp != wgpu::StencilOperation::Keep ||
               mDepthStencil->stencilFront.depthFailOp != wgpu::StencilOperation::Keep ||
               mDepthStencil->stencilFront.passOp != wgpu::StencilOperation::Keep;
    }

    bool BlendEnabled(const ColorStateDescriptor* mColorState) {
        return mColorState->alphaBlend.operation != wgpu::BlendOperation::Add ||
               mColorState->alphaBlend.srcFactor != wgpu::BlendFactor::One ||
               mColorState->alphaBlend.dstFactor != wgpu::BlendFactor::Zero ||
               mColorState->colorBlend.operation != wgpu::BlendOperation::Add ||
               mColorState->colorBlend.srcFactor != wgpu::BlendFactor::One ||
               mColorState->colorBlend.dstFactor != wgpu::BlendFactor::Zero;
    }

    // RenderPipelineBase

    RenderPipelineBase::RenderPipelineBase(DeviceBase* device,
                                           const RenderPipelineDescriptor* descriptor)
        : PipelineBase(device,
                       descriptor->layout,
                       {{SingleShaderStage::Vertex, &descriptor->vertexStage},
                        {SingleShaderStage::Fragment, descriptor->fragmentStage}}),
          mAttachmentState(device->GetOrCreateAttachmentState(descriptor)) {
        mPrimitive.topology = descriptor->primitiveTopology;

        mMultisample.count = descriptor->sampleCount;
        mMultisample.mask = descriptor->sampleMask;
        mMultisample.alphaToCoverageEnabled = descriptor->alphaToCoverageEnabled;

        if (descriptor->vertexState != nullptr) {
            const VertexStateDescriptor& vertexState = *descriptor->vertexState;
            mVertexBufferCount = vertexState.vertexBufferCount;
            mPrimitive.stripIndexFormat = vertexState.indexFormat;

            for (uint8_t slot = 0; slot < mVertexBufferCount; ++slot) {
                if (vertexState.vertexBuffers[slot].attributeCount == 0) {
                    continue;
                }

                VertexBufferSlot typedSlot(slot);

                mVertexBufferSlotsUsed.set(typedSlot);
                mVertexBufferInfos[typedSlot].arrayStride =
                    vertexState.vertexBuffers[slot].arrayStride;
                mVertexBufferInfos[typedSlot].stepMode = vertexState.vertexBuffers[slot].stepMode;

                for (uint32_t i = 0; i < vertexState.vertexBuffers[slot].attributeCount; ++i) {
                    VertexAttributeLocation location = VertexAttributeLocation(static_cast<uint8_t>(
                        vertexState.vertexBuffers[slot].attributes[i].shaderLocation));
                    mAttributeLocationsUsed.set(location);
                    mAttributeInfos[location].shaderLocation = location;
                    mAttributeInfos[location].vertexBufferSlot = typedSlot;
                    mAttributeInfos[location].offset =
                        vertexState.vertexBuffers[slot].attributes[i].offset;

                    mAttributeInfos[location].format = dawn::NormalizeVertexFormat(
                        vertexState.vertexBuffers[slot].attributes[i].format);
                }
            }
        } else {
            mVertexBufferCount = 0;
            mPrimitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
        }

        if (mAttachmentState->HasDepthStencilAttachment()) {
            const DepthStencilStateDescriptor& depthStencil = *descriptor->depthStencilState;
            mDepthStencil.nextInChain = depthStencil.nextInChain;
            mDepthStencil.format = depthStencil.format;
            mDepthStencil.depthWriteEnabled = depthStencil.depthWriteEnabled;
            mDepthStencil.depthCompare = depthStencil.depthCompare;
            mDepthStencil.stencilBack = depthStencil.stencilBack;
            mDepthStencil.stencilFront = depthStencil.stencilFront;
            mDepthStencil.stencilReadMask = depthStencil.stencilReadMask;
            mDepthStencil.stencilWriteMask = depthStencil.stencilWriteMask;
        } else {
            // These default values below are useful for backends to fill information.
            // The values indicate that depth and stencil test are disabled when backends
            // set their own depth stencil states/descriptors according to the values in
            // mDepthStencil.
            mDepthStencil.format = wgpu::TextureFormat::Undefined;
            mDepthStencil.depthWriteEnabled = false;
            mDepthStencil.depthCompare = wgpu::CompareFunction::Always;
            mDepthStencil.stencilBack.compare = wgpu::CompareFunction::Always;
            mDepthStencil.stencilBack.failOp = wgpu::StencilOperation::Keep;
            mDepthStencil.stencilBack.depthFailOp = wgpu::StencilOperation::Keep;
            mDepthStencil.stencilBack.passOp = wgpu::StencilOperation::Keep;
            mDepthStencil.stencilFront.compare = wgpu::CompareFunction::Always;
            mDepthStencil.stencilFront.failOp = wgpu::StencilOperation::Keep;
            mDepthStencil.stencilFront.depthFailOp = wgpu::StencilOperation::Keep;
            mDepthStencil.stencilFront.passOp = wgpu::StencilOperation::Keep;
            mDepthStencil.stencilReadMask = 0xff;
            mDepthStencil.stencilWriteMask = 0xff;
        }

        if (descriptor->rasterizationState != nullptr) {
            mPrimitive.frontFace = descriptor->rasterizationState->frontFace;
            mPrimitive.cullMode = descriptor->rasterizationState->cullMode;
            mDepthStencil.depthBias = descriptor->rasterizationState->depthBias;
            mDepthStencil.depthBiasSlopeScale = descriptor->rasterizationState->depthBiasSlopeScale;
            mDepthStencil.depthBiasClamp = descriptor->rasterizationState->depthBiasClamp;
        } else {
            mPrimitive.frontFace = wgpu::FrontFace::CCW;
            mPrimitive.cullMode = wgpu::CullMode::None;
            mDepthStencil.depthBias = 0;
            mDepthStencil.depthBiasSlopeScale = 0.0f;
            mDepthStencil.depthBiasClamp = 0.0f;
        }

        for (ColorAttachmentIndex i : IterateBitSet(mAttachmentState->GetColorAttachmentsMask())) {
            const ColorStateDescriptor* colorState =
                &descriptor->colorStates[static_cast<uint8_t>(i)];
            mTargets[i].format = colorState->format;
            mTargets[i].writeMask = colorState->writeMask;

            if (BlendEnabled(colorState)) {
                mTargetBlend[i].color = colorState->colorBlend;
                mTargetBlend[i].alpha = colorState->alphaBlend;
                mTargets[i].blend = &mTargetBlend[i];
            } else {
                mTargets[i].blend = nullptr;
            }
        }

        // TODO(cwallez@chromium.org): Check against the shader module that the correct color
        // attachment are set?
    }

    RenderPipelineBase::RenderPipelineBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : PipelineBase(device, tag) {
    }

    // static
    RenderPipelineBase* RenderPipelineBase::MakeError(DeviceBase* device) {
        return new RenderPipelineBase(device, ObjectBase::kError);
    }

    RenderPipelineBase::~RenderPipelineBase() {
        if (IsCachedReference()) {
            GetDevice()->UncacheRenderPipeline(this);
        }
    }

    const ityp::bitset<VertexAttributeLocation, kMaxVertexAttributes>&
    RenderPipelineBase::GetAttributeLocationsUsed() const {
        ASSERT(!IsError());
        return mAttributeLocationsUsed;
    }

    const VertexAttributeInfo& RenderPipelineBase::GetAttribute(
        VertexAttributeLocation location) const {
        ASSERT(!IsError());
        ASSERT(mAttributeLocationsUsed[location]);
        return mAttributeInfos[location];
    }

    const ityp::bitset<VertexBufferSlot, kMaxVertexBuffers>&
    RenderPipelineBase::GetVertexBufferSlotsUsed() const {
        ASSERT(!IsError());
        return mVertexBufferSlotsUsed;
    }

    const VertexBufferInfo& RenderPipelineBase::GetVertexBuffer(VertexBufferSlot slot) const {
        ASSERT(!IsError());
        ASSERT(mVertexBufferSlotsUsed[slot]);
        return mVertexBufferInfos[slot];
    }

    uint32_t RenderPipelineBase::GetVertexBufferCount() const {
        ASSERT(!IsError());
        return mVertexBufferCount;
    }

    const ColorTargetState* RenderPipelineBase::GetColorTargetState(
        ColorAttachmentIndex attachmentSlot) const {
        ASSERT(!IsError());
        ASSERT(attachmentSlot < mTargets.size());
        return &mTargets[attachmentSlot];
    }

    const DepthStencilState* RenderPipelineBase::GetDepthStencilState() const {
        ASSERT(!IsError());
        return &mDepthStencil;
    }

    wgpu::PrimitiveTopology RenderPipelineBase::GetPrimitiveTopology() const {
        ASSERT(!IsError());
        return mPrimitive.topology;
    }

    wgpu::IndexFormat RenderPipelineBase::GetStripIndexFormat() const {
        ASSERT(!IsError());
        return mPrimitive.stripIndexFormat;
    }

    wgpu::CullMode RenderPipelineBase::GetCullMode() const {
        ASSERT(!IsError());
        return mPrimitive.cullMode;
    }

    wgpu::FrontFace RenderPipelineBase::GetFrontFace() const {
        ASSERT(!IsError());
        return mPrimitive.frontFace;
    }

    bool RenderPipelineBase::IsDepthBiasEnabled() const {
        ASSERT(!IsError());
        return mDepthStencil.depthBias != 0 || mDepthStencil.depthBiasSlopeScale != 0;
    }

    int32_t RenderPipelineBase::GetDepthBias() const {
        ASSERT(!IsError());
        return mDepthStencil.depthBias;
    }

    float RenderPipelineBase::GetDepthBiasSlopeScale() const {
        ASSERT(!IsError());
        return mDepthStencil.depthBiasSlopeScale;
    }

    float RenderPipelineBase::GetDepthBiasClamp() const {
        ASSERT(!IsError());
        return mDepthStencil.depthBiasClamp;
    }

    bool RenderPipelineBase::ShouldClampDepth() const {
        ASSERT(!IsError());
        const ChainedStruct* chained = mDepthStencil.nextInChain;
        if (chained == nullptr) {
            return false;
        }
        ASSERT(chained->sType == wgpu::SType::DepthStencilDepthClampingState);
        const auto* depthClampingState =
            static_cast<const DepthStencilDepthClampingState*>(chained);
        return depthClampingState->clampDepth;
    }

    ityp::bitset<ColorAttachmentIndex, kMaxColorAttachments>
    RenderPipelineBase::GetColorAttachmentsMask() const {
        ASSERT(!IsError());
        return mAttachmentState->GetColorAttachmentsMask();
    }

    bool RenderPipelineBase::HasDepthStencilAttachment() const {
        ASSERT(!IsError());
        return mAttachmentState->HasDepthStencilAttachment();
    }

    wgpu::TextureFormat RenderPipelineBase::GetColorAttachmentFormat(
        ColorAttachmentIndex attachment) const {
        ASSERT(!IsError());
        return mTargets[attachment].format;
    }

    wgpu::TextureFormat RenderPipelineBase::GetDepthStencilFormat() const {
        ASSERT(!IsError());
        ASSERT(mAttachmentState->HasDepthStencilAttachment());
        return mDepthStencil.format;
    }

    uint32_t RenderPipelineBase::GetSampleCount() const {
        ASSERT(!IsError());
        return mAttachmentState->GetSampleCount();
    }

    uint32_t RenderPipelineBase::GetSampleMask() const {
        ASSERT(!IsError());
        return mMultisample.mask;
    }

    bool RenderPipelineBase::IsAlphaToCoverageEnabled() const {
        ASSERT(!IsError());
        return mMultisample.alphaToCoverageEnabled;
    }

    const AttachmentState* RenderPipelineBase::GetAttachmentState() const {
        ASSERT(!IsError());

        return mAttachmentState.Get();
    }

    size_t RenderPipelineBase::ComputeContentHash() {
        ObjectContentHasher recorder;

        // Record modules and layout
        recorder.Record(PipelineBase::ComputeContentHash());

        // Hierarchically record the attachment state.
        // It contains the attachments set, texture formats, and sample count.
        recorder.Record(mAttachmentState->GetContentHash());

        // Record attachments
        for (ColorAttachmentIndex i : IterateBitSet(mAttachmentState->GetColorAttachmentsMask())) {
            const ColorTargetState& desc = *GetColorTargetState(i);
            recorder.Record(desc.writeMask);
            if (desc.blend != nullptr) {
                recorder.Record(desc.blend->color.operation, desc.blend->color.srcFactor,
                                desc.blend->color.dstFactor);
                recorder.Record(desc.blend->alpha.operation, desc.blend->alpha.srcFactor,
                                desc.blend->alpha.dstFactor);
            }
        }

        if (mAttachmentState->HasDepthStencilAttachment()) {
            const DepthStencilState& desc = mDepthStencil;
            if (desc.nextInChain == nullptr ||
                desc.nextInChain->sType != wgpu::SType::DepthStencilDepthClampingState) {
                recorder.Record(false);
            } else {
                const auto* chained =
                    static_cast<const DepthStencilDepthClampingState*>(desc.nextInChain);
                recorder.Record(chained->clampDepth);
            }
            recorder.Record(desc.depthWriteEnabled, desc.depthCompare);
            recorder.Record(desc.stencilReadMask, desc.stencilWriteMask);
            recorder.Record(desc.stencilFront.compare, desc.stencilFront.failOp,
                            desc.stencilFront.depthFailOp, desc.stencilFront.passOp);
            recorder.Record(desc.stencilBack.compare, desc.stencilBack.failOp,
                            desc.stencilBack.depthFailOp, desc.stencilBack.passOp);
            recorder.Record(desc.depthBias, desc.depthBiasSlopeScale, desc.depthBiasClamp);
        }

        // Record vertex state
        recorder.Record(mAttributeLocationsUsed);
        for (VertexAttributeLocation location : IterateBitSet(mAttributeLocationsUsed)) {
            const VertexAttributeInfo& desc = GetAttribute(location);
            recorder.Record(desc.shaderLocation, desc.vertexBufferSlot, desc.offset, desc.format);
        }

        recorder.Record(mVertexBufferSlotsUsed);
        for (VertexBufferSlot slot : IterateBitSet(mVertexBufferSlotsUsed)) {
            const VertexBufferInfo& desc = GetVertexBuffer(slot);
            recorder.Record(desc.arrayStride, desc.stepMode);
        }

        // Record primitive state
        recorder.Record(mPrimitive.topology, mPrimitive.stripIndexFormat, mPrimitive.frontFace,
                        mPrimitive.cullMode);

        // Record multisample state
        // Sample count hashed as part of the attachment state
        recorder.Record(mMultisample.mask, mMultisample.alphaToCoverageEnabled);

        return recorder.GetContentHash();
    }

    bool RenderPipelineBase::EqualityFunc::operator()(const RenderPipelineBase* a,
                                                      const RenderPipelineBase* b) const {
        // Check the layout and shader stages.
        if (!PipelineBase::EqualForCache(a, b)) {
            return false;
        }

        // Check the attachment state.
        // It contains the attachments set, texture formats, and sample count.
        if (a->mAttachmentState.Get() != b->mAttachmentState.Get()) {
            return false;
        }

        for (ColorAttachmentIndex i :
             IterateBitSet(a->mAttachmentState->GetColorAttachmentsMask())) {
            const ColorTargetState& descA = *a->GetColorTargetState(i);
            const ColorTargetState& descB = *b->GetColorTargetState(i);
            if (descA.writeMask != descB.writeMask) {
                return false;
            }
            if ((descA.blend == nullptr) != (descB.blend == nullptr)) {
                return false;
            }
            if (descA.blend != nullptr) {
                if (descA.blend->color.operation != descB.blend->color.operation ||
                    descA.blend->color.srcFactor != descB.blend->color.srcFactor ||
                    descA.blend->color.dstFactor != descB.blend->color.dstFactor) {
                    return false;
                }
                if (descA.blend->alpha.operation != descB.blend->alpha.operation ||
                    descA.blend->alpha.srcFactor != descB.blend->alpha.srcFactor ||
                    descA.blend->alpha.dstFactor != descB.blend->alpha.dstFactor) {
                    return false;
                }
            }
        }

        // Check depth/stencil state
        if (a->mAttachmentState->HasDepthStencilAttachment()) {
            const DepthStencilState& stateA = a->mDepthStencil;
            const DepthStencilState& stateB = b->mDepthStencil;

            ASSERT(!std::isnan(stateA.depthBiasSlopeScale));
            ASSERT(!std::isnan(stateB.depthBiasSlopeScale));
            ASSERT(!std::isnan(stateA.depthBiasClamp));
            ASSERT(!std::isnan(stateB.depthBiasClamp));

            if (stateA.depthWriteEnabled != stateB.depthWriteEnabled ||
                stateA.depthCompare != stateB.depthCompare ||
                stateA.depthBias != stateB.depthBias ||
                stateA.depthBiasSlopeScale != stateB.depthBiasSlopeScale ||
                stateA.depthBiasClamp != stateB.depthBiasClamp) {
                return false;
            }
            if (stateA.stencilFront.compare != stateB.stencilFront.compare ||
                stateA.stencilFront.failOp != stateB.stencilFront.failOp ||
                stateA.stencilFront.depthFailOp != stateB.stencilFront.depthFailOp ||
                stateA.stencilFront.passOp != stateB.stencilFront.passOp) {
                return false;
            }
            if (stateA.stencilBack.compare != stateB.stencilBack.compare ||
                stateA.stencilBack.failOp != stateB.stencilBack.failOp ||
                stateA.stencilBack.depthFailOp != stateB.stencilBack.depthFailOp ||
                stateA.stencilBack.passOp != stateB.stencilBack.passOp) {
                return false;
            }
            if (stateA.stencilReadMask != stateB.stencilReadMask ||
                stateA.stencilWriteMask != stateB.stencilWriteMask) {
                return false;
            }
        }

        // Check vertex state
        if (a->mAttributeLocationsUsed != b->mAttributeLocationsUsed) {
            return false;
        }

        for (VertexAttributeLocation loc : IterateBitSet(a->mAttributeLocationsUsed)) {
            const VertexAttributeInfo& descA = a->GetAttribute(loc);
            const VertexAttributeInfo& descB = b->GetAttribute(loc);
            if (descA.shaderLocation != descB.shaderLocation ||
                descA.vertexBufferSlot != descB.vertexBufferSlot || descA.offset != descB.offset ||
                descA.format != descB.format) {
                return false;
            }
        }

        if (a->mVertexBufferSlotsUsed != b->mVertexBufferSlotsUsed) {
            return false;
        }

        for (VertexBufferSlot slot : IterateBitSet(a->mVertexBufferSlotsUsed)) {
            const VertexBufferInfo& descA = a->GetVertexBuffer(slot);
            const VertexBufferInfo& descB = b->GetVertexBuffer(slot);
            if (descA.arrayStride != descB.arrayStride || descA.stepMode != descB.stepMode) {
                return false;
            }
        }

        // Check primitive state
        {
            const PrimitiveState& stateA = a->mPrimitive;
            const PrimitiveState& stateB = b->mPrimitive;
            if (stateA.topology != stateB.topology ||
                stateA.stripIndexFormat != stateB.stripIndexFormat ||
                stateA.frontFace != stateB.frontFace || stateA.cullMode != stateB.cullMode) {
                return false;
            }
        }

        // Check multisample state
        {
            const MultisampleState& stateA = a->mMultisample;
            const MultisampleState& stateB = b->mMultisample;
            // Sample count already checked as part of the attachment state.
            if (stateA.mask != stateB.mask ||
                stateA.alphaToCoverageEnabled != stateB.alphaToCoverageEnabled) {
                return false;
            }
        }

        return true;
    }

}  // namespace dawn_native
