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

#include "dawn_native/CommandEncoder.h"

#include "common/BitSetIterator.h"
#include "common/Math.h"
#include "dawn_native/BindGroup.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandBuffer.h"
#include "dawn_native/CommandBufferStateTracker.h"
#include "dawn_native/CommandValidation.h"
#include "dawn_native/Commands.h"
#include "dawn_native/ComputePassEncoder.h"
#include "dawn_native/ComputePipeline.h"
#include "dawn_native/Device.h"
#include "dawn_native/ErrorData.h"
#include "dawn_native/QueryHelper.h"
#include "dawn_native/QuerySet.h"
#include "dawn_native/Queue.h"
#include "dawn_native/RenderPassEncoder.h"
#include "dawn_native/RenderPipeline.h"
#include "dawn_native/ValidationUtils_autogen.h"
#include "dawn_platform/DawnPlatform.h"
#include "dawn_platform/tracing/TraceEvent.h"

#include <cmath>
#include <map>

namespace dawn_native {

    namespace {

        MaybeError ValidateB2BCopyAlignment(uint64_t dataSize,
                                            uint64_t srcOffset,
                                            uint64_t dstOffset) {
            // Copy size must be a multiple of 4 bytes on macOS.
            if (dataSize % 4 != 0) {
                return DAWN_VALIDATION_ERROR("Copy size must be a multiple of 4 bytes");
            }

            // SourceOffset and destinationOffset must be multiples of 4 bytes on macOS.
            if (srcOffset % 4 != 0 || dstOffset % 4 != 0) {
                return DAWN_VALIDATION_ERROR(
                    "Source offset and destination offset must be multiples of 4 bytes");
            }

            return {};
        }

        MaybeError ValidateTextureSampleCountInBufferCopyCommands(const TextureBase* texture) {
            if (texture->GetSampleCount() > 1) {
                return DAWN_VALIDATION_ERROR(
                    "The sample count of textures must be 1 when copying between buffers and "
                    "textures");
            }

            return {};
        }

        MaybeError ValidateLinearTextureCopyOffset(const TextureDataLayout& layout,
                                                   const TexelBlockInfo& blockInfo) {
            if (layout.offset % blockInfo.byteSize != 0) {
                return DAWN_VALIDATION_ERROR(
                    "offset must be a multiple of the texel block byte size.");
            }
            return {};
        }

        MaybeError ValidateTextureDepthStencilToBufferCopyRestrictions(
            const ImageCopyTexture& src) {
            Aspect aspectUsed;
            DAWN_TRY_ASSIGN(aspectUsed, SingleAspectUsedByImageCopyTexture(src));
            if (aspectUsed == Aspect::Depth) {
                switch (src.texture->GetFormat().format) {
                    case wgpu::TextureFormat::Depth24Plus:
                    case wgpu::TextureFormat::Depth24PlusStencil8:
                        return DAWN_VALIDATION_ERROR(
                            "The depth aspect of depth24plus texture cannot be selected in a "
                            "texture to buffer copy");
                        break;
                    case wgpu::TextureFormat::Depth32Float:
                        break;

                    default:
                        UNREACHABLE();
                }
            }

            return {};
        }

        MaybeError ValidateAttachmentArrayLayersAndLevelCount(const TextureViewBase* attachment) {
            // Currently we do not support layered rendering.
            if (attachment->GetLayerCount() > 1) {
                return DAWN_VALIDATION_ERROR(
                    "The layer count of the texture view used as attachment cannot be greater than "
                    "1");
            }

            if (attachment->GetLevelCount() > 1) {
                return DAWN_VALIDATION_ERROR(
                    "The mipmap level count of the texture view used as attachment cannot be "
                    "greater than 1");
            }

            return {};
        }

        MaybeError ValidateOrSetAttachmentSize(const TextureViewBase* attachment,
                                               uint32_t* width,
                                               uint32_t* height) {
            const Extent3D& attachmentSize =
                attachment->GetTexture()->GetMipLevelVirtualSize(attachment->GetBaseMipLevel());

            if (*width == 0) {
                DAWN_ASSERT(*height == 0);
                *width = attachmentSize.width;
                *height = attachmentSize.height;
                DAWN_ASSERT(*width != 0 && *height != 0);
            } else if (*width != attachmentSize.width || *height != attachmentSize.height) {
                return DAWN_VALIDATION_ERROR("Attachment size mismatch");
            }

            return {};
        }

        MaybeError ValidateOrSetColorAttachmentSampleCount(const TextureViewBase* colorAttachment,
                                                           uint32_t* sampleCount) {
            if (*sampleCount == 0) {
                *sampleCount = colorAttachment->GetTexture()->GetSampleCount();
                DAWN_ASSERT(*sampleCount != 0);
            } else if (*sampleCount != colorAttachment->GetTexture()->GetSampleCount()) {
                return DAWN_VALIDATION_ERROR("Color attachment sample counts mismatch");
            }

            return {};
        }

        MaybeError ValidateResolveTarget(
            const DeviceBase* device,
            const RenderPassColorAttachmentDescriptor& colorAttachment) {
            if (colorAttachment.resolveTarget == nullptr) {
                return {};
            }

            const TextureViewBase* resolveTarget = colorAttachment.resolveTarget;
            const TextureViewBase* attachment =
                colorAttachment.view != nullptr ? colorAttachment.view : colorAttachment.attachment;
            DAWN_TRY(device->ValidateObject(colorAttachment.resolveTarget));
            DAWN_TRY(ValidateCanUseAs(colorAttachment.resolveTarget->GetTexture(),
                                      wgpu::TextureUsage::RenderAttachment));

            if (!attachment->GetTexture()->IsMultisampledTexture()) {
                return DAWN_VALIDATION_ERROR(
                    "Cannot set resolve target when the sample count of the color attachment is 1");
            }

            if (resolveTarget->GetTexture()->IsMultisampledTexture()) {
                return DAWN_VALIDATION_ERROR("Cannot use multisampled texture as resolve target");
            }

            if (resolveTarget->GetLayerCount() > 1) {
                return DAWN_VALIDATION_ERROR(
                    "The array layer count of the resolve target must be 1");
            }

            if (resolveTarget->GetLevelCount() > 1) {
                return DAWN_VALIDATION_ERROR("The mip level count of the resolve target must be 1");
            }

            const Extent3D& colorTextureSize =
                attachment->GetTexture()->GetMipLevelVirtualSize(attachment->GetBaseMipLevel());
            const Extent3D& resolveTextureSize =
                resolveTarget->GetTexture()->GetMipLevelVirtualSize(
                    resolveTarget->GetBaseMipLevel());
            if (colorTextureSize.width != resolveTextureSize.width ||
                colorTextureSize.height != resolveTextureSize.height) {
                return DAWN_VALIDATION_ERROR(
                    "The size of the resolve target must be the same as the color attachment");
            }

            wgpu::TextureFormat resolveTargetFormat = resolveTarget->GetFormat().format;
            if (resolveTargetFormat != attachment->GetFormat().format) {
                return DAWN_VALIDATION_ERROR(
                    "The format of the resolve target must be the same as the color attachment");
            }

            return {};
        }

        MaybeError ValidateRenderPassColorAttachment(
            DeviceBase* device,
            const RenderPassColorAttachmentDescriptor& colorAttachment,
            uint32_t* width,
            uint32_t* height,
            uint32_t* sampleCount) {
            TextureViewBase* attachment;
            if (colorAttachment.view != nullptr) {
                if (colorAttachment.attachment != nullptr) {
                    return DAWN_VALIDATION_ERROR(
                        "Cannot specify both a attachment and view. attachment is deprecated, "
                        "favor view instead.");
                }
                attachment = colorAttachment.view;
            } else if (colorAttachment.attachment != nullptr) {
                device->EmitDeprecationWarning(
                    "RenderPassColorAttachmentDescriptor.attachment has been deprecated. Use "
                    "RenderPassColorAttachmentDescriptor.view instead.");
                attachment = colorAttachment.attachment;
            } else {
                return DAWN_VALIDATION_ERROR(
                    "Must specify a view for RenderPassColorAttachmentDescriptor");
            }

            DAWN_TRY(device->ValidateObject(attachment));
            DAWN_TRY(
                ValidateCanUseAs(attachment->GetTexture(), wgpu::TextureUsage::RenderAttachment));

            if (!(attachment->GetAspects() & Aspect::Color) ||
                !attachment->GetFormat().isRenderable) {
                return DAWN_VALIDATION_ERROR(
                    "The format of the texture view used as color attachment is not color "
                    "renderable");
            }

            DAWN_TRY(ValidateLoadOp(colorAttachment.loadOp));
            DAWN_TRY(ValidateStoreOp(colorAttachment.storeOp));

            if (colorAttachment.loadOp == wgpu::LoadOp::Clear) {
                if (std::isnan(colorAttachment.clearColor.r) ||
                    std::isnan(colorAttachment.clearColor.g) ||
                    std::isnan(colorAttachment.clearColor.b) ||
                    std::isnan(colorAttachment.clearColor.a)) {
                    return DAWN_VALIDATION_ERROR("Color clear value cannot contain NaN");
                }
            }

            DAWN_TRY(ValidateOrSetColorAttachmentSampleCount(attachment, sampleCount));

            DAWN_TRY(ValidateResolveTarget(device, colorAttachment));

            DAWN_TRY(ValidateAttachmentArrayLayersAndLevelCount(attachment));
            DAWN_TRY(ValidateOrSetAttachmentSize(attachment, width, height));

            return {};
        }

        MaybeError ValidateRenderPassDepthStencilAttachment(
            DeviceBase* device,
            const RenderPassDepthStencilAttachmentDescriptor* depthStencilAttachment,
            uint32_t* width,
            uint32_t* height,
            uint32_t* sampleCount) {
            DAWN_ASSERT(depthStencilAttachment != nullptr);

            TextureViewBase* attachment;
            if (depthStencilAttachment->view != nullptr) {
                if (depthStencilAttachment->attachment != nullptr) {
                    return DAWN_VALIDATION_ERROR(
                        "Cannot specify both a attachment and view. attachment is deprecated, "
                        "favor view instead.");
                }
                attachment = depthStencilAttachment->view;
            } else if (depthStencilAttachment->attachment != nullptr) {
                device->EmitDeprecationWarning(
                    "RenderPassDepthStencilAttachmentDescriptor.attachment has been deprecated. "
                    "Use RenderPassDepthStencilAttachmentDescriptor.view instead.");
                attachment = depthStencilAttachment->attachment;
            } else {
                return DAWN_VALIDATION_ERROR(
                    "Must specify a view for RenderPassDepthStencilAttachmentDescriptor");
            }

            DAWN_TRY(device->ValidateObject(attachment));
            DAWN_TRY(
                ValidateCanUseAs(attachment->GetTexture(), wgpu::TextureUsage::RenderAttachment));

            if ((attachment->GetAspects() & (Aspect::Depth | Aspect::Stencil)) == Aspect::None ||
                !attachment->GetFormat().isRenderable) {
                return DAWN_VALIDATION_ERROR(
                    "The format of the texture view used as depth stencil attachment is not a "
                    "depth stencil format");
            }

            DAWN_TRY(ValidateLoadOp(depthStencilAttachment->depthLoadOp));
            DAWN_TRY(ValidateLoadOp(depthStencilAttachment->stencilLoadOp));
            DAWN_TRY(ValidateStoreOp(depthStencilAttachment->depthStoreOp));
            DAWN_TRY(ValidateStoreOp(depthStencilAttachment->stencilStoreOp));

            if (attachment->GetAspects() == (Aspect::Depth | Aspect::Stencil) &&
                depthStencilAttachment->depthReadOnly != depthStencilAttachment->stencilReadOnly) {
                return DAWN_VALIDATION_ERROR(
                    "depthReadOnly and stencilReadOnly must be the same when texture aspect is "
                    "'all'");
            }

            if (depthStencilAttachment->depthReadOnly &&
                (depthStencilAttachment->depthLoadOp != wgpu::LoadOp::Load ||
                 depthStencilAttachment->depthStoreOp != wgpu::StoreOp::Store)) {
                return DAWN_VALIDATION_ERROR(
                    "depthLoadOp must be load and depthStoreOp must be store when depthReadOnly "
                    "is true.");
            }

            if (depthStencilAttachment->stencilReadOnly &&
                (depthStencilAttachment->stencilLoadOp != wgpu::LoadOp::Load ||
                 depthStencilAttachment->stencilStoreOp != wgpu::StoreOp::Store)) {
                return DAWN_VALIDATION_ERROR(
                    "stencilLoadOp must be load and stencilStoreOp must be store when "
                    "stencilReadOnly "
                    "is true.");
            }

            if (depthStencilAttachment->depthLoadOp == wgpu::LoadOp::Clear &&
                std::isnan(depthStencilAttachment->clearDepth)) {
                return DAWN_VALIDATION_ERROR("Depth clear value cannot be NaN");
            }

            // *sampleCount == 0 must only happen when there is no color attachment. In that case we
            // do not need to validate the sample count of the depth stencil attachment.
            const uint32_t depthStencilSampleCount = attachment->GetTexture()->GetSampleCount();
            if (*sampleCount != 0) {
                if (depthStencilSampleCount != *sampleCount) {
                    return DAWN_VALIDATION_ERROR("Depth stencil attachment sample counts mismatch");
                }
            } else {
                *sampleCount = depthStencilSampleCount;
            }

            DAWN_TRY(ValidateAttachmentArrayLayersAndLevelCount(attachment));
            DAWN_TRY(ValidateOrSetAttachmentSize(attachment, width, height));

            return {};
        }

        MaybeError ValidateRenderPassDescriptor(DeviceBase* device,
                                                const RenderPassDescriptor* descriptor,
                                                uint32_t* width,
                                                uint32_t* height,
                                                uint32_t* sampleCount) {
            if (descriptor->colorAttachmentCount > kMaxColorAttachments) {
                return DAWN_VALIDATION_ERROR("Setting color attachments out of bounds");
            }

            for (uint32_t i = 0; i < descriptor->colorAttachmentCount; ++i) {
                DAWN_TRY(ValidateRenderPassColorAttachment(device, descriptor->colorAttachments[i],
                                                           width, height, sampleCount));
            }

            if (descriptor->depthStencilAttachment != nullptr) {
                DAWN_TRY(ValidateRenderPassDepthStencilAttachment(
                    device, descriptor->depthStencilAttachment, width, height, sampleCount));
            }

            if (descriptor->occlusionQuerySet != nullptr) {
                DAWN_TRY(device->ValidateObject(descriptor->occlusionQuerySet));

                if (descriptor->occlusionQuerySet->GetQueryType() != wgpu::QueryType::Occlusion) {
                    return DAWN_VALIDATION_ERROR("The type of query set must be Occlusion");
                }
            }

            if (descriptor->colorAttachmentCount == 0 &&
                descriptor->depthStencilAttachment == nullptr) {
                return DAWN_VALIDATION_ERROR("Cannot use render pass with no attachments.");
            }

            return {};
        }

        MaybeError ValidateComputePassDescriptor(const DeviceBase* device,
                                                 const ComputePassDescriptor* descriptor) {
            return {};
        }

        MaybeError ValidateQuerySetResolve(const QuerySetBase* querySet,
                                           uint32_t firstQuery,
                                           uint32_t queryCount,
                                           const BufferBase* destination,
                                           uint64_t destinationOffset) {
            if (firstQuery >= querySet->GetQueryCount()) {
                return DAWN_VALIDATION_ERROR("Query index out of bounds");
            }

            if (queryCount > querySet->GetQueryCount() - firstQuery) {
                return DAWN_VALIDATION_ERROR(
                    "The sum of firstQuery and queryCount exceeds the number of queries in query "
                    "set");
            }

            // The destinationOffset must be a multiple of 8 bytes on D3D12 and Vulkan
            if (destinationOffset % 8 != 0) {
                return DAWN_VALIDATION_ERROR(
                    "The alignment offset into the destination buffer must be a multiple of 8 "
                    "bytes");
            }

            uint64_t bufferSize = destination->GetSize();
            // The destination buffer must have enough storage, from destination offset, to contain
            // the result of resolved queries
            bool fitsInBuffer = destinationOffset <= bufferSize &&
                                (static_cast<uint64_t>(queryCount) * sizeof(uint64_t) <=
                                 (bufferSize - destinationOffset));
            if (!fitsInBuffer) {
                return DAWN_VALIDATION_ERROR("The resolved query data would overflow the buffer");
            }

            return {};
        }

        MaybeError EncodeTimestampsToNanosecondsConversion(CommandEncoder* encoder,
                                                           QuerySetBase* querySet,
                                                           uint32_t firstQuery,
                                                           uint32_t queryCount,
                                                           BufferBase* destination,
                                                           uint64_t destinationOffset) {
            DeviceBase* device = encoder->GetDevice();

            // The availability got from query set is a reference to vector<bool>, need to covert
            // bool to uint32_t due to a user input in pipeline must not contain a bool type in
            // WGSL.
            std::vector<uint32_t> availability{querySet->GetQueryAvailability().begin(),
                                               querySet->GetQueryAvailability().end()};

            // Timestamp availability storage buffer
            BufferDescriptor availabilityDesc = {};
            availabilityDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
            availabilityDesc.size = querySet->GetQueryCount() * sizeof(uint32_t);
            Ref<BufferBase> availabilityBuffer;
            DAWN_TRY_ASSIGN(availabilityBuffer, device->CreateBuffer(&availabilityDesc));

            DAWN_TRY(device->GetQueue()->WriteBuffer(availabilityBuffer.Get(), 0,
                                                     availability.data(),
                                                     availability.size() * sizeof(uint32_t)));

            // Timestamp params uniform buffer
            TimestampParams params = {firstQuery, queryCount,
                                      static_cast<uint32_t>(destinationOffset),
                                      device->GetTimestampPeriodInNS()};

            BufferDescriptor parmsDesc = {};
            parmsDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
            parmsDesc.size = sizeof(params);
            Ref<BufferBase> paramsBuffer;
            DAWN_TRY_ASSIGN(paramsBuffer, device->CreateBuffer(&parmsDesc));

            DAWN_TRY(
                device->GetQueue()->WriteBuffer(paramsBuffer.Get(), 0, &params, sizeof(params)));

            return EncodeConvertTimestampsToNanoseconds(
                encoder, destination, availabilityBuffer.Get(), paramsBuffer.Get());
        }

    }  // namespace

    CommandEncoder::CommandEncoder(DeviceBase* device, const CommandEncoderDescriptor*)
        : ObjectBase(device), mEncodingContext(device, this) {
    }

    CommandBufferResourceUsage CommandEncoder::AcquireResourceUsages() {
        return CommandBufferResourceUsage{
            mEncodingContext.AcquireRenderPassUsages(), mEncodingContext.AcquireComputePassUsages(),
            std::move(mTopLevelBuffers), std::move(mTopLevelTextures), std::move(mUsedQuerySets)};
    }

    CommandIterator CommandEncoder::AcquireCommands() {
        return mEncodingContext.AcquireCommands();
    }

    void CommandEncoder::TrackUsedQuerySet(QuerySetBase* querySet) {
        mUsedQuerySets.insert(querySet);
    }

    void CommandEncoder::TrackQueryAvailability(QuerySetBase* querySet, uint32_t queryIndex) {
        DAWN_ASSERT(querySet != nullptr);

        if (GetDevice()->IsValidationEnabled()) {
            TrackUsedQuerySet(querySet);
        }

        // Set the query at queryIndex to available for resolving in query set.
        querySet->SetQueryAvailability(queryIndex, true);
    }

    // Implementation of the API's command recording methods

    ComputePassEncoder* CommandEncoder::APIBeginComputePass(
        const ComputePassDescriptor* descriptor) {
        DeviceBase* device = GetDevice();

        bool success =
            mEncodingContext.TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
                DAWN_TRY(ValidateComputePassDescriptor(device, descriptor));

                allocator->Allocate<BeginComputePassCmd>(Command::BeginComputePass);

                return {};
            });

        if (success) {
            ComputePassEncoder* passEncoder =
                new ComputePassEncoder(device, this, &mEncodingContext);
            mEncodingContext.EnterPass(passEncoder);
            return passEncoder;
        }

        return ComputePassEncoder::MakeError(device, this, &mEncodingContext);
    }

    RenderPassEncoder* CommandEncoder::APIBeginRenderPass(const RenderPassDescriptor* descriptor) {
        DeviceBase* device = GetDevice();

        RenderPassResourceUsageTracker usageTracker;

        uint32_t width = 0;
        uint32_t height = 0;
        Ref<AttachmentState> attachmentState;
        bool success =
            mEncodingContext.TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
                uint32_t sampleCount = 0;

                DAWN_TRY(ValidateRenderPassDescriptor(device, descriptor, &width, &height,
                                                      &sampleCount));

                ASSERT(width > 0 && height > 0 && sampleCount > 0);

                BeginRenderPassCmd* cmd =
                    allocator->Allocate<BeginRenderPassCmd>(Command::BeginRenderPass);

                cmd->attachmentState = device->GetOrCreateAttachmentState(descriptor);
                attachmentState = cmd->attachmentState;

                for (ColorAttachmentIndex index :
                     IterateBitSet(cmd->attachmentState->GetColorAttachmentsMask())) {
                    uint8_t i = static_cast<uint8_t>(index);
                    TextureViewBase* view = descriptor->colorAttachments[i].view;
                    if (view == nullptr) {
                        view = descriptor->colorAttachments[i].attachment;
                    }
                    TextureViewBase* resolveTarget = descriptor->colorAttachments[i].resolveTarget;

                    cmd->colorAttachments[index].view = view;
                    cmd->colorAttachments[index].resolveTarget = resolveTarget;
                    cmd->colorAttachments[index].loadOp = descriptor->colorAttachments[i].loadOp;
                    cmd->colorAttachments[index].storeOp = descriptor->colorAttachments[i].storeOp;
                    cmd->colorAttachments[index].clearColor =
                        descriptor->colorAttachments[i].clearColor;

                    usageTracker.TextureViewUsedAs(view, wgpu::TextureUsage::RenderAttachment);

                    if (resolveTarget != nullptr) {
                        usageTracker.TextureViewUsedAs(resolveTarget,
                                                       wgpu::TextureUsage::RenderAttachment);
                    }
                }

                if (cmd->attachmentState->HasDepthStencilAttachment()) {
                    TextureViewBase* view = descriptor->depthStencilAttachment->view;
                    if (view == nullptr) {
                        view = descriptor->depthStencilAttachment->attachment;
                    }

                    cmd->depthStencilAttachment.view = view;
                    cmd->depthStencilAttachment.clearDepth =
                        descriptor->depthStencilAttachment->clearDepth;
                    cmd->depthStencilAttachment.clearStencil =
                        descriptor->depthStencilAttachment->clearStencil;
                    cmd->depthStencilAttachment.depthLoadOp =
                        descriptor->depthStencilAttachment->depthLoadOp;
                    cmd->depthStencilAttachment.depthStoreOp =
                        descriptor->depthStencilAttachment->depthStoreOp;
                    cmd->depthStencilAttachment.stencilLoadOp =
                        descriptor->depthStencilAttachment->stencilLoadOp;
                    cmd->depthStencilAttachment.stencilStoreOp =
                        descriptor->depthStencilAttachment->stencilStoreOp;

                    usageTracker.TextureViewUsedAs(view, wgpu::TextureUsage::RenderAttachment);
                }

                cmd->width = width;
                cmd->height = height;

                cmd->occlusionQuerySet = descriptor->occlusionQuerySet;

                return {};
            });

        if (success) {
            RenderPassEncoder* passEncoder = new RenderPassEncoder(
                device, this, &mEncodingContext, std::move(usageTracker), attachmentState,
                descriptor->occlusionQuerySet, width, height);
            mEncodingContext.EnterPass(passEncoder);

            /*if (attachmentState->HasDepthStencilAttachment() &&
                device->IsToggleEnabled(Toggle::IDK)) {
                TextureViewBase* view = descriptor->depthStencilAttachment->view;
                if (view == nullptr) {
                    view = descriptor->depthStencilAttachment->attachment;
                }
                Aspect aspectsToClear = Aspect::None;
                if (descriptor->depthStencilAttachment->depthLoadOp == wgpu::LoadOp::Clear) {
                    aspectsToClear |= (view->GetAspects() & Aspect::Depth);
                }
                if (descriptor->depthStencilAttachment->stencilLoadOp == wgpu::LoadOp::Clear) {
                    aspectsToClear |= (view->GetAspects() & Aspect::Stencil);
                }
                if (aspectsToClear != Aspect::None) {
                    passEncoder->EncodeClearDSWithQuad(
                        view, aspectsToClear, descriptor->depthStencilAttachment->clearDepth,
                        descriptor->depthStencilAttachment->clearStencil,
                        std::move(attachmentState));
                }
            }*/

            return passEncoder;
        }

        return RenderPassEncoder::MakeError(device, this, &mEncodingContext);
    }

    void CommandEncoder::APICopyBufferToBuffer(BufferBase* source,
                                               uint64_t sourceOffset,
                                               BufferBase* destination,
                                               uint64_t destinationOffset,
                                               uint64_t size) {
        mEncodingContext.TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            if (GetDevice()->IsValidationEnabled()) {
                DAWN_TRY(GetDevice()->ValidateObject(source));
                DAWN_TRY(GetDevice()->ValidateObject(destination));

                if (source == destination) {
                    return DAWN_VALIDATION_ERROR(
                        "Source and destination cannot be the same buffer.");
                }

                DAWN_TRY(ValidateCopySizeFitsInBuffer(source, sourceOffset, size));
                DAWN_TRY(ValidateCopySizeFitsInBuffer(destination, destinationOffset, size));
                DAWN_TRY(ValidateB2BCopyAlignment(size, sourceOffset, destinationOffset));

                DAWN_TRY(ValidateCanUseAs(source, wgpu::BufferUsage::CopySrc));
                DAWN_TRY(ValidateCanUseAs(destination, wgpu::BufferUsage::CopyDst));

                mTopLevelBuffers.insert(source);
                mTopLevelBuffers.insert(destination);
            }

            CopyBufferToBufferCmd* copy =
                allocator->Allocate<CopyBufferToBufferCmd>(Command::CopyBufferToBuffer);
            copy->source = source;
            copy->sourceOffset = sourceOffset;
            copy->destination = destination;
            copy->destinationOffset = destinationOffset;
            copy->size = size;

            return {};
        });
    }

    void CommandEncoder::APICopyBufferToTexture(const ImageCopyBuffer* source,
                                                const ImageCopyTexture* destination,
                                                const Extent3D* copySize) {
        mEncodingContext.TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            if (GetDevice()->IsValidationEnabled()) {
                DAWN_TRY(ValidateImageCopyBuffer(GetDevice(), *source));
                DAWN_TRY(ValidateCanUseAs(source->buffer, wgpu::BufferUsage::CopySrc));

                DAWN_TRY(ValidateImageCopyTexture(GetDevice(), *destination, *copySize));
                DAWN_TRY(ValidateCanUseAs(destination->texture, wgpu::TextureUsage::CopyDst));
                DAWN_TRY(ValidateTextureSampleCountInBufferCopyCommands(destination->texture));

                DAWN_TRY(ValidateLinearToDepthStencilCopyRestrictions(*destination));
                // We validate texture copy range before validating linear texture data,
                // because in the latter we divide copyExtent.width by blockWidth and
                // copyExtent.height by blockHeight while the divisibility conditions are
                // checked in validating texture copy range.
                DAWN_TRY(ValidateTextureCopyRange(GetDevice(), *destination, *copySize));
            }
            const TexelBlockInfo& blockInfo =
                destination->texture->GetFormat().GetAspectInfo(destination->aspect).block;
            TextureDataLayout srcLayout = FixUpDeprecatedTextureDataLayoutOptions(
                GetDevice(), source->layout, blockInfo, *copySize);
            if (GetDevice()->IsValidationEnabled()) {
                DAWN_TRY(ValidateLinearTextureCopyOffset(srcLayout, blockInfo));
                DAWN_TRY(ValidateLinearTextureData(srcLayout, source->buffer->GetSize(), blockInfo,
                                                   *copySize));

                mTopLevelBuffers.insert(source->buffer);
                mTopLevelTextures.insert(destination->texture);
            }

            ApplyDefaultTextureDataLayoutOptions(&srcLayout, blockInfo, *copySize);

            CopyBufferToTextureCmd* copy =
                allocator->Allocate<CopyBufferToTextureCmd>(Command::CopyBufferToTexture);
            copy->source.buffer = source->buffer;
            copy->source.offset = srcLayout.offset;
            copy->source.bytesPerRow = srcLayout.bytesPerRow;
            copy->source.rowsPerImage = srcLayout.rowsPerImage;
            copy->destination.texture = destination->texture;
            copy->destination.origin = destination->origin;
            copy->destination.mipLevel = destination->mipLevel;
            copy->destination.aspect =
                ConvertAspect(destination->texture->GetFormat(), destination->aspect);
            copy->copySize = *copySize;

            return {};
        });
    }

    void CommandEncoder::APICopyTextureToBuffer(const ImageCopyTexture* source,
                                                const ImageCopyBuffer* destination,
                                                const Extent3D* copySize) {
        mEncodingContext.TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            if (GetDevice()->IsValidationEnabled()) {
                DAWN_TRY(ValidateImageCopyTexture(GetDevice(), *source, *copySize));
                DAWN_TRY(ValidateCanUseAs(source->texture, wgpu::TextureUsage::CopySrc));
                DAWN_TRY(ValidateTextureSampleCountInBufferCopyCommands(source->texture));
                DAWN_TRY(ValidateTextureDepthStencilToBufferCopyRestrictions(*source));

                DAWN_TRY(ValidateImageCopyBuffer(GetDevice(), *destination));
                DAWN_TRY(ValidateCanUseAs(destination->buffer, wgpu::BufferUsage::CopyDst));

                // We validate texture copy range before validating linear texture data,
                // because in the latter we divide copyExtent.width by blockWidth and
                // copyExtent.height by blockHeight while the divisibility conditions are
                // checked in validating texture copy range.
                DAWN_TRY(ValidateTextureCopyRange(GetDevice(), *source, *copySize));
            }
            const TexelBlockInfo& blockInfo =
                source->texture->GetFormat().GetAspectInfo(source->aspect).block;
            TextureDataLayout dstLayout = FixUpDeprecatedTextureDataLayoutOptions(
                GetDevice(), destination->layout, blockInfo, *copySize);
            if (GetDevice()->IsValidationEnabled()) {
                DAWN_TRY(ValidateLinearTextureCopyOffset(dstLayout, blockInfo));
                DAWN_TRY(ValidateLinearTextureData(dstLayout, destination->buffer->GetSize(),
                                                   blockInfo, *copySize));

                mTopLevelTextures.insert(source->texture);
                mTopLevelBuffers.insert(destination->buffer);
            }

            ApplyDefaultTextureDataLayoutOptions(&dstLayout, blockInfo, *copySize);

            Aspect aspect = ConvertAspect(source->texture->GetFormat(), source->aspect);
            if ((aspect & (Aspect::Depth | Aspect::Stencil)) != Aspect::None &&
                GetDevice()->IsToggleEnabled(Toggle::IDK)) {
                GetDevice()->ConsumedError(EncodeBlitDSTextureToBuffer(
                    allocator, source, aspect, destination->buffer, dstLayout, *copySize));
                return {};
            }

            CopyTextureToBufferCmd* copy =
                allocator->Allocate<CopyTextureToBufferCmd>(Command::CopyTextureToBuffer);
            copy->source.texture = source->texture;
            copy->source.origin = source->origin;
            copy->source.mipLevel = source->mipLevel;
            copy->source.aspect = ConvertAspect(source->texture->GetFormat(), source->aspect);
            copy->destination.buffer = destination->buffer;
            copy->destination.offset = dstLayout.offset;
            copy->destination.bytesPerRow = dstLayout.bytesPerRow;
            copy->destination.rowsPerImage = dstLayout.rowsPerImage;
            copy->copySize = *copySize;

            return {};
        });
    }

    void CommandEncoder::APICopyTextureToTexture(const ImageCopyTexture* source,
                                                 const ImageCopyTexture* destination,
                                                 const Extent3D* copySize) {
        mEncodingContext.TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            if (GetDevice()->IsValidationEnabled()) {
                DAWN_TRY(GetDevice()->ValidateObject(source->texture));
                DAWN_TRY(GetDevice()->ValidateObject(destination->texture));

                DAWN_TRY(ValidateImageCopyTexture(GetDevice(), *source, *copySize));
                DAWN_TRY(ValidateImageCopyTexture(GetDevice(), *destination, *copySize));

                DAWN_TRY(
                    ValidateTextureToTextureCopyRestrictions(*source, *destination, *copySize));

                DAWN_TRY(ValidateTextureCopyRange(GetDevice(), *source, *copySize));
                DAWN_TRY(ValidateTextureCopyRange(GetDevice(), *destination, *copySize));

                DAWN_TRY(ValidateCanUseAs(source->texture, wgpu::TextureUsage::CopySrc));
                DAWN_TRY(ValidateCanUseAs(destination->texture, wgpu::TextureUsage::CopyDst));

                mTopLevelTextures.insert(source->texture);
                mTopLevelTextures.insert(destination->texture);
            }

            // Aspect aspect = ConvertAspect(source->texture->GetFormat(), source->aspect);
            // if ((aspect & (Aspect::Depth | Aspect::Stencil)) != Aspect::None &&
            //     GetDevice()->IsToggleEnabled(Toggle::IDK)) {
            //     GetDevice()->ConsumedError(EncodeBlitDSTextureToTexture(
            //         allocator, source, aspect, destination, *copySize));
            //     return {};
            // }

            CopyTextureToTextureCmd* copy =
                allocator->Allocate<CopyTextureToTextureCmd>(Command::CopyTextureToTexture);
            copy->source.texture = source->texture;
            copy->source.origin = source->origin;
            copy->source.mipLevel = source->mipLevel;
            copy->source.aspect = ConvertAspect(source->texture->GetFormat(), source->aspect);
            copy->destination.texture = destination->texture;
            copy->destination.origin = destination->origin;
            copy->destination.mipLevel = destination->mipLevel;
            copy->destination.aspect =
                ConvertAspect(destination->texture->GetFormat(), destination->aspect);
            copy->copySize = *copySize;

            return {};
        });
    }

    void CommandEncoder::APIInjectValidationError(const char* message) {
        if (mEncodingContext.CheckCurrentEncoder(this)) {
            mEncodingContext.HandleError(DAWN_VALIDATION_ERROR(message));
        }
    }

    void CommandEncoder::APIInsertDebugMarker(const char* groupLabel) {
        mEncodingContext.TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            InsertDebugMarkerCmd* cmd =
                allocator->Allocate<InsertDebugMarkerCmd>(Command::InsertDebugMarker);
            cmd->length = strlen(groupLabel);

            char* label = allocator->AllocateData<char>(cmd->length + 1);
            memcpy(label, groupLabel, cmd->length + 1);

            return {};
        });
    }

    void CommandEncoder::APIPopDebugGroup() {
        mEncodingContext.TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            if (GetDevice()->IsValidationEnabled()) {
                if (mDebugGroupStackSize == 0) {
                    return DAWN_VALIDATION_ERROR("Pop must be balanced by a corresponding Push.");
                }
            }
            allocator->Allocate<PopDebugGroupCmd>(Command::PopDebugGroup);
            mDebugGroupStackSize--;

            return {};
        });
    }

    void CommandEncoder::APIPushDebugGroup(const char* groupLabel) {
        mEncodingContext.TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            PushDebugGroupCmd* cmd =
                allocator->Allocate<PushDebugGroupCmd>(Command::PushDebugGroup);
            cmd->length = strlen(groupLabel);

            char* label = allocator->AllocateData<char>(cmd->length + 1);
            memcpy(label, groupLabel, cmd->length + 1);

            mDebugGroupStackSize++;

            return {};
        });
    }

    void CommandEncoder::APIResolveQuerySet(QuerySetBase* querySet,
                                            uint32_t firstQuery,
                                            uint32_t queryCount,
                                            BufferBase* destination,
                                            uint64_t destinationOffset) {
        mEncodingContext.TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            if (GetDevice()->IsValidationEnabled()) {
                DAWN_TRY(GetDevice()->ValidateObject(querySet));
                DAWN_TRY(GetDevice()->ValidateObject(destination));

                DAWN_TRY(ValidateQuerySetResolve(querySet, firstQuery, queryCount, destination,
                                                 destinationOffset));

                DAWN_TRY(ValidateCanUseAs(destination, wgpu::BufferUsage::QueryResolve));

                TrackUsedQuerySet(querySet);
                mTopLevelBuffers.insert(destination);
            }

            ResolveQuerySetCmd* cmd =
                allocator->Allocate<ResolveQuerySetCmd>(Command::ResolveQuerySet);
            cmd->querySet = querySet;
            cmd->firstQuery = firstQuery;
            cmd->queryCount = queryCount;
            cmd->destination = destination;
            cmd->destinationOffset = destinationOffset;

            // Encode internal compute pipeline for timestamp query
            if (querySet->GetQueryType() == wgpu::QueryType::Timestamp) {
                DAWN_TRY(EncodeTimestampsToNanosecondsConversion(
                    this, querySet, firstQuery, queryCount, destination, destinationOffset));
            }

            return {};
        });
    }

    void CommandEncoder::APIWriteTimestamp(QuerySetBase* querySet, uint32_t queryIndex) {
        mEncodingContext.TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            if (GetDevice()->IsValidationEnabled()) {
                DAWN_TRY(GetDevice()->ValidateObject(querySet));
                DAWN_TRY(ValidateTimestampQuery(querySet, queryIndex));
            }

            TrackQueryAvailability(querySet, queryIndex);

            WriteTimestampCmd* cmd =
                allocator->Allocate<WriteTimestampCmd>(Command::WriteTimestamp);
            cmd->querySet = querySet;
            cmd->queryIndex = queryIndex;

            return {};
        });
    }

    CommandBufferBase* CommandEncoder::APIFinish(const CommandBufferDescriptor* descriptor) {
        Ref<CommandBufferBase> commandBuffer;
        if (GetDevice()->ConsumedError(FinishInternal(descriptor), &commandBuffer)) {
            return CommandBufferBase::MakeError(GetDevice());
        }
        ASSERT(!IsError());
        return commandBuffer.Detach();
    }

    ResultOrError<Ref<CommandBufferBase>> CommandEncoder::FinishInternal(
        const CommandBufferDescriptor* descriptor) {
        DeviceBase* device = GetDevice();

        // Even if mEncodingContext.Finish() validation fails, calling it will mutate the internal
        // state of the encoding context. The internal state is set to finished, and subsequent
        // calls to encode commands will generate errors.
        DAWN_TRY(mEncodingContext.Finish());
        DAWN_TRY(device->ValidateIsAlive());

        if (device->IsValidationEnabled()) {
            DAWN_TRY(ValidateFinish());
        }
        return device->CreateCommandBuffer(this, descriptor);
    }

    // Implementation of the command buffer validation that can be precomputed before submit
    MaybeError CommandEncoder::ValidateFinish() const {
        TRACE_EVENT0(GetDevice()->GetPlatform(), Validation, "CommandEncoder::ValidateFinish");
        DAWN_TRY(GetDevice()->ValidateObject(this));

        for (const RenderPassResourceUsage& passUsage : mEncodingContext.GetRenderPassUsages()) {
            DAWN_TRY(ValidateSyncScopeResourceUsage(passUsage));
        }

        for (const ComputePassResourceUsage& passUsage : mEncodingContext.GetComputePassUsages()) {
            for (const SyncScopeResourceUsage& scope : passUsage.dispatchUsages) {
                DAWN_TRY(ValidateSyncScopeResourceUsage(scope));
            }
        }

        if (mDebugGroupStackSize != 0) {
            return DAWN_VALIDATION_ERROR("Each Push must be balanced by a corresponding Pop.");
        }

        return {};
    }

    MaybeError CommandEncoder::EncodeBlitDSTextureToBuffer(CommandAllocator* allocator,
                                                           const ImageCopyTexture* source,
                                                           Aspect aspect,
                                                           BufferBase* dstBuffer,
                                                           const TextureDataLayout& dstLayout,
                                                           const Extent3D& copySize) {
        DeviceBase* device = GetDevice();
        const TexelBlockInfo& blockInfo =
            source->texture->GetFormat().GetAspectInfo(aspect).block;

        Ref<ShaderModuleBase> shaderModule;

        switch (aspect) {
            case Aspect::Depth: {
                ShaderModuleWGSLDescriptor smWgslDesc = {};
                ShaderModuleDescriptor smDesc = {};
                smDesc.nextInChain = &smWgslDesc;
                smWgslDesc.source = R"(
                    [[block]] struct Params {
                        [[size(4)]] width : u32;
                        [[size(4)]] bytesPerRow : u32;
                        [[size(4)]] rowsPerImage : u32;
                        [[size(4)]] offsetX: u32;
                        [[size(4)]] offsetY: u32;
                        [[size(4)]] level : u32;
                    };

                    [[block]] struct Result {
                        values : array<f32>;
                    };

                    [[group(0), binding(0)]] var input : texture_depth_2d_array;
                    [[group(0), binding(1)]] var<uniform> params : Params;
                    [[group(0), binding(2)]] var<storage, read_write> result : Result;

                    [[workgroup_size(1, 1, 1)]]
                    [[stage(compute)]] fn main(
                        [[builtin(global_invocation_id)]] global_id: vec3<u32>
                    ) {
                        let offset : vec3<u32> = global_id + vec3<u32>(params.offsetX, params.offsetY, 0u);
                        let value : f32 = textureLoad(
                            input,
                            vec2<i32>(i32(offset.x), i32(offset.y)),
                            i32(offset.z),
                            i32(params.level)
                        );

                        let widthInTexels : u32 = params.bytesPerRow / 4u;
                        result.values[
                            global_id.x +
                            global_id.y * widthInTexels +
                            global_id.z * widthInTexels * params.rowsPerImage] = value;
                    }
                )";
                DAWN_TRY_ASSIGN(shaderModule, device->CreateShaderModule(&smDesc, nullptr));
                break;
            }
            case Aspect::Stencil: {
                ShaderModuleWGSLDescriptor smWgslDesc = {};
                ShaderModuleDescriptor smDesc = {};
                smDesc.nextInChain = &smWgslDesc;
                smWgslDesc.source = R"(
                    [[block]] struct Params {
                        [[size(4)]] width : u32;
                        [[size(4)]] bytesPerRow : u32;
                        [[size(4)]] rowsPerImage : u32;
                        [[size(4)]] offsetX: u32;
                        [[size(4)]] offsetY: u32;
                        [[size(4)]] level : u32;
                    };

                    [[block]] struct Result {
                        values : array<u32>;
                    };

                    [[group(0), binding(0)]] var input : texture_2d_array<u32>;
                    [[group(0), binding(1)]] var<uniform> params : Params;
                    [[group(0), binding(2)]] var<storage, read_write> result : Result;

                    [[workgroup_size(1, 1, 1)]]
                    [[stage(compute)]] fn main(
                        [[builtin(global_invocation_id)]] global_id: vec3<u32>
                    ) {
                        let offset : vec3<u32> = global_id + vec3<u32>(params.offsetX, params.offsetY, 0u);
                        var value0 : u32;
                        var value1 : u32;
                        var value2 : u32;
                        var value3 : u32;
                        if (4u * offset.x + 0u < params.width) {
                            value0 = textureLoad(
                                input,
                                vec2<i32>(i32(4u * offset.x + 0u), i32(offset.y)),
                                i32(offset.z),
                                i32(params.level)
                            )[0];
                        }
                        if (4u * offset.x + 1u < params.width) {
                            value1 = textureLoad(
                                input,
                                vec2<i32>(i32(4u * offset.x + 1u), i32(offset.y)),
                                i32(offset.z),
                                i32(params.level)
                            )[0];
                        }
                        if (4u * offset.x + 2u < params.width) {
                            value2 = textureLoad(
                                input,
                                vec2<i32>(i32(4u * offset.x + 2u), i32(offset.y)),
                                i32(offset.z),
                                i32(params.level)
                            )[0];
                        }
                        if (4u * offset.x + 3u < params.width) {
                            value3 = textureLoad(
                                input,
                                vec2<i32>(i32(4u * offset.x + 3u), i32(offset.y)),
                                i32(offset.z),
                                i32(params.level)
                            )[0];
                        }

                        let widthIn4Texels : u32 = params.bytesPerRow / 4u;
                        let index : u32 =
                            global_id.x +
                            global_id.y * widthIn4Texels +
                            global_id.z * widthIn4Texels * params.rowsPerImage;

                        result.values[index] = (
                            ((value0 & 0x000000FFu) << 0u) +
                            ((value1 & 0x000000FFu) << 8u) +
                            ((value2 & 0x000000FFu) << 16u) +
                            ((value3 & 0x000000FFu) << 24u)
                        );
                    }
                )";
                DAWN_TRY_ASSIGN(shaderModule, device->CreateShaderModule(&smDesc, nullptr));
                break;
            }
            default:
                UNREACHABLE();
        }

        ComputePipelineDescriptor cpDesc = {};
        cpDesc.compute.module = shaderModule.Get();
        cpDesc.compute.entryPoint = "main";

        Ref<ComputePipelineBase> pipeline;
        DAWN_TRY_ASSIGN(pipeline, device->CreateComputePipeline(&cpDesc));

        Ref<BindGroupLayoutBase> bgl;
        DAWN_TRY_ASSIGN(bgl, pipeline->GetBindGroupLayout(0));

        TextureViewDescriptor inputViewDesc = {};
        inputViewDesc.dimension = wgpu::TextureViewDimension::e2DArray;
        inputViewDesc.aspect = source->aspect;
        inputViewDesc.baseArrayLayer = source->origin.z;
        inputViewDesc.arrayLayerCount = copySize.depthOrArrayLayers;
        // inputViewDesc.baseMipLevel = source->mipLevel;
        // inputViewDesc.mipLevelCount = 1;

        Ref<TextureViewBase> inputView;
        DAWN_TRY_ASSIGN(inputView, device->CreateTextureView(source->texture, &inputViewDesc));

        uint32_t bytesPerRow = std::max(Align(
            blockInfo.byteSize * copySize.width,
            device->GetOptimalBytesPerRowAlignment()), 16u);
        uint32_t rowsPerImage = copySize.height;
        // uint32_t bytesPerRow = dstLayout.bytesPerRow;
        // uint32_t rowsPerImage = dstLayout.rowsPerImage;

        struct Params {
            uint32_t width;
            uint32_t bytesPerRow;
            uint32_t rowsPerImage;
            uint32_t offsetX;
            uint32_t offsetY;
            uint32_t mipLevel;
        };
        static_assert(sizeof(Params) == 24, "");
        Params params = {
            copySize.width,
            bytesPerRow,
            rowsPerImage,
            source->origin.x,
            source->origin.y,
            source->mipLevel,
            // 0,
        };
        BufferDescriptor uniformBufferDesc = {};
        uniformBufferDesc.size = sizeof(Params);
        uniformBufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;

        Ref<BufferBase> uniformBuffer;
        DAWN_TRY_ASSIGN(uniformBuffer, device->CreateBuffer(&uniformBufferDesc));
        DAWN_TRY(device->GetQueue()->WriteBuffer(uniformBuffer.Get(), 0, &params, sizeof(Params)));

        BufferDescriptor resultBufferDesc = {};
        resultBufferDesc.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
        resultBufferDesc.size =
            bytesPerRow * rowsPerImage * copySize.depthOrArrayLayers;

        Ref<BufferBase> resultBuffer;
        DAWN_TRY_ASSIGN(resultBuffer, device->CreateBuffer(&resultBufferDesc));
        std::vector<uint8_t> zeroBuffer(resultBufferDesc.size);
        DAWN_TRY(device->GetQueue()->WriteBuffer(resultBuffer.Get(), 0, zeroBuffer.data(), zeroBuffer.size()));

        std::array<BindGroupEntry, 3> entries = {};
        entries[0].binding = 0;
        entries[0].textureView = inputView.Get();
        entries[1].binding = 1;
        entries[1].buffer = uniformBuffer.Get();
        entries[1].size = uniformBufferDesc.size;
        entries[2].binding = 2;
        entries[2].buffer = resultBuffer.Get();
        entries[2].size = resultBufferDesc.size;

        BindGroupDescriptor bgDesc = {};
        bgDesc.layout = bgl.Get();
        bgDesc.entryCount = entries.size();
        bgDesc.entries = entries.data();

        Ref<BindGroupBase> bg;
        DAWN_TRY_ASSIGN(bg, device->UnsafeCreateBindGroup(&bgDesc));

        ComputePassEncoder* passEncoder = APIBeginComputePass(nullptr);
        passEncoder->APISetPipeline(pipeline.Get());
        passEncoder->APISetBindGroup(0, bg.Get());

        switch (aspect) {
            case Aspect::Depth:
                passEncoder->APIDispatch(copySize.width, copySize.height,
                                         copySize.depthOrArrayLayers);
                break;
            case Aspect::Stencil:
                passEncoder->APIDispatch((copySize.width + 3) / 4, copySize.height,
                                         copySize.depthOrArrayLayers);
                break;
            default:
                UNREACHABLE();
        }

        passEncoder->APIEndPass();

        // Copy row-by-row.
        for (uint32_t z = 0; z < copySize.depthOrArrayLayers; ++z) {
            for (uint32_t y = 0; y < copySize.height; ++y) {
                CopyBufferToBufferCmd* copy =
                    allocator->Allocate<CopyBufferToBufferCmd>(Command::CopyBufferToBuffer);
                copy->source = resultBuffer.Get();
                copy->sourceOffset = bytesPerRow * rowsPerImage * z +
                                     bytesPerRow * y;
                copy->destination = dstBuffer;
                copy->destinationOffset = dstLayout.offset +
                                          dstLayout.bytesPerRow * dstLayout.rowsPerImage * z +
                                          dstLayout.bytesPerRow * y;
                copy->size = blockInfo.byteSize * copySize.width;
            }
        }

        // CopyBufferToTextureCmd* copy =
        //     allocator->Allocate<CopyBufferToTextureCmd>(Command::CopyBufferToTexture);
        // copy->source.buffer = resultBuffer.Get();
        // copy->source.offset = 0;
        // copy->source.bytesPerRow = dstLayout.bytesPerRow;
        // copy->source.rowsPerImage = dstLayout.rowsPerImage;
        // copy->destination.texture = source->texture;
        // copy->destination.origin = source->origin;
        // copy->destination.mipLevel = source->mipLevel;
        // copy->destination.aspect = aspect;
        // copy->copySize = copySize;

        return {};
    }

    MaybeError CommandEncoder::EncodeBlitDSTextureToTexture(CommandAllocator* allocator,
                                                           const ImageCopyTexture* source,
                                                           Aspect aspects,
                                                           const ImageCopyTexture* destination,
                                                           const Extent3D& copySize) {
        DeviceBase* device = GetDevice();
        for (Aspect aspect : IterateEnumMask(aspects)) {
            const TexelBlockInfo& blockInfo =
                source->texture->GetFormat().GetAspectInfo(aspect).block;

            Ref<ShaderModuleBase> shaderModule;

            switch (aspect) {
                case Aspect::Depth: {
                    ShaderModuleWGSLDescriptor smWgslDesc = {};
                    ShaderModuleDescriptor smDesc = {};
                    smDesc.nextInChain = &smWgslDesc;
                    smWgslDesc.source = R"(
                        [[block]] struct Params {
                            [[size(4)]] width : u32;
                            [[size(4)]] bytesPerRow : u32;
                            [[size(4)]] rowsPerImage : u32;
                            [[size(4)]] offsetX: u32;
                            [[size(4)]] offsetY: u32;
                            [[size(4)]] level : u32;
                        };

                        [[block]] struct Result {
                            values : array<f32>;
                        };

                        [[group(0), binding(0)]] var input : texture_depth_2d_array;
                        [[group(0), binding(1)]] var<uniform> params : Params;
                        [[group(0), binding(2)]] var<storage, read_write> result : Result;

                        [[workgroup_size(1, 1, 1)]]
                        [[stage(compute)]] fn main(
                            [[builtin(global_invocation_id)]] global_id: vec3<u32>
                        ) {
                            let offset : vec3<u32> = global_id + vec3<u32>(params.offsetX, params.offsetY, 0u);
                            let value : f32 = textureLoad(
                                input,
                                vec2<i32>(i32(offset.x), i32(offset.y)),
                                i32(offset.z),
                                i32(params.level)
                            );

                            let widthInTexels : u32 = params.bytesPerRow / 4u;
                            result.values[
                                global_id.x +
                                global_id.y * widthInTexels +
                                global_id.z * widthInTexels * params.rowsPerImage] = value;
                        }
                    )";
                    DAWN_TRY_ASSIGN(shaderModule, device->CreateShaderModule(&smDesc, nullptr));
                    break;
                }
                case Aspect::Stencil: {
                    ShaderModuleWGSLDescriptor smWgslDesc = {};
                    ShaderModuleDescriptor smDesc = {};
                    smDesc.nextInChain = &smWgslDesc;
                    smWgslDesc.source = R"(
                        [[block]] struct Params {
                            [[size(4)]] width : u32;
                            [[size(4)]] bytesPerRow : u32;
                            [[size(4)]] rowsPerImage : u32;
                            [[size(4)]] offsetX: u32;
                            [[size(4)]] offsetY: u32;
                            [[size(4)]] level : u32;
                        };

                        [[block]] struct Result {
                            values : array<u32>;
                        };

                        [[group(0), binding(0)]] var input : texture_2d_array<u32>;
                        [[group(0), binding(1)]] var<uniform> params : Params;
                        [[group(0), binding(2)]] var<storage, read_write> result : Result;

                        [[workgroup_size(1, 1, 1)]]
                        [[stage(compute)]] fn main(
                            [[builtin(global_invocation_id)]] global_id: vec3<u32>
                        ) {
                            let offset : vec3<u32> = global_id + vec3<u32>(params.offsetX, params.offsetY, 0u);
                            var value0 : u32;
                            var value1 : u32;
                            var value2 : u32;
                            var value3 : u32;
                            if (4u * offset.x + 0u < params.width) {
                                value0 = textureLoad(
                                    input,
                                    vec2<i32>(i32(4u * offset.x + 0u), i32(offset.y)),
                                    i32(offset.z),
                                    i32(params.level)
                                )[0];
                            }
                            if (4u * offset.x + 1u < params.width) {
                                value1 = textureLoad(
                                    input,
                                    vec2<i32>(i32(4u * offset.x + 1u), i32(offset.y)),
                                    i32(offset.z),
                                    i32(params.level)
                                )[0];
                            }
                            if (4u * offset.x + 2u < params.width) {
                                value2 = textureLoad(
                                    input,
                                    vec2<i32>(i32(4u * offset.x + 2u), i32(offset.y)),
                                    i32(offset.z),
                                    i32(params.level)
                                )[0];
                            }
                            if (4u * offset.x + 3u < params.width) {
                                value3 = textureLoad(
                                    input,
                                    vec2<i32>(i32(4u * offset.x + 3u), i32(offset.y)),
                                    i32(offset.z),
                                    i32(params.level)
                                )[0];
                            }

                            let widthIn4Texels : u32 = params.bytesPerRow / 4u;
                            let index : u32 =
                                global_id.x +
                                global_id.y * widthIn4Texels +
                                global_id.z * widthIn4Texels * params.rowsPerImage;

                            result.values[index] = (
                                ((value0 & 0x000000FFu) << 0u) +
                                ((value1 & 0x000000FFu) << 8u) +
                                ((value2 & 0x000000FFu) << 16u) +
                                ((value3 & 0x000000FFu) << 24u)
                            );
                        }
                    )";
                    DAWN_TRY_ASSIGN(shaderModule, device->CreateShaderModule(&smDesc, nullptr));
                    break;
                }
                default:
                    UNREACHABLE();
            }

            ComputePipelineDescriptor cpDesc = {};
            cpDesc.compute.module = shaderModule.Get();
            cpDesc.compute.entryPoint = "main";

            Ref<ComputePipelineBase> pipeline;
            DAWN_TRY_ASSIGN(pipeline, device->CreateComputePipeline(&cpDesc));

            Ref<BindGroupLayoutBase> bgl;
            DAWN_TRY_ASSIGN(bgl, pipeline->GetBindGroupLayout(0));

            TextureViewDescriptor inputViewDesc = {};
            inputViewDesc.dimension = wgpu::TextureViewDimension::e2DArray;
            switch (aspect) {
                case Aspect::Depth:
                    inputViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
                    break;
                case Aspect::Stencil:
                    inputViewDesc.aspect = wgpu::TextureAspect::StencilOnly;
                    break;
                default:
                    UNREACHABLE();
            }
            inputViewDesc.baseArrayLayer = source->origin.z;
            inputViewDesc.arrayLayerCount = copySize.depthOrArrayLayers;
            // inputViewDesc.baseMipLevel = source->mipLevel;
            // inputViewDesc.mipLevelCount = 1;

            Ref<TextureViewBase> inputView;
            DAWN_TRY_ASSIGN(inputView, device->CreateTextureView(source->texture, &inputViewDesc));

             uint32_t bytesPerRow = std::max(Align(
                blockInfo.byteSize * copySize.width,
                device->GetOptimalBytesPerRowAlignment()), 16u);
            uint32_t rowsPerImage = copySize.height;
            // uint32_t bytesPerRow = std::max(Align(copySize.width device->GetOptimalBytesPerRowAlignment(), 16u);
            // uint32_t rowsPerImage = copySize.height;

            struct Params {
                uint32_t width;
                uint32_t bytesPerRow;
                uint32_t rowsPerImage;
                uint32_t offsetX;
                uint32_t offsetY;
                uint32_t mipLevel;
            };
            static_assert(sizeof(Params) == 24, "");
            Params params = {
                copySize.width,
                bytesPerRow,
                rowsPerImage,
                source->origin.x,
                source->origin.y,
                source->mipLevel,
                // 0,
            };
            BufferDescriptor uniformBufferDesc = {};
            uniformBufferDesc.size = sizeof(Params);
            uniformBufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;

            Ref<BufferBase> uniformBuffer;
            DAWN_TRY_ASSIGN(uniformBuffer, device->CreateBuffer(&uniformBufferDesc));
            DAWN_TRY(device->GetQueue()->WriteBuffer(uniformBuffer.Get(), 0, &params, sizeof(Params)));

            BufferDescriptor resultBufferDesc = {};
            resultBufferDesc.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::Storage;
            resultBufferDesc.size =
                bytesPerRow * rowsPerImage * copySize.depthOrArrayLayers;

            Ref<BufferBase> resultBuffer;
            DAWN_TRY_ASSIGN(resultBuffer, device->CreateBuffer(&resultBufferDesc));

            std::array<BindGroupEntry, 3> entries = {};
            entries[0].binding = 0;
            entries[0].textureView = inputView.Get();
            entries[1].binding = 1;
            entries[1].buffer = uniformBuffer.Get();
            entries[1].size = uniformBufferDesc.size;
            entries[2].binding = 2;
            entries[2].buffer = resultBuffer.Get();
            entries[2].size = resultBufferDesc.size;

            BindGroupDescriptor bgDesc = {};
            bgDesc.layout = bgl.Get();
            bgDesc.entryCount = entries.size();
            bgDesc.entries = entries.data();

            Ref<BindGroupBase> bg;
            DAWN_TRY_ASSIGN(bg, device->UnsafeCreateBindGroup(&bgDesc));

            ComputePassEncoder* passEncoder = APIBeginComputePass(nullptr);
            passEncoder->APISetPipeline(pipeline.Get());
            passEncoder->APISetBindGroup(0, bg.Get());

            switch (aspect) {
                case Aspect::Depth:
                    passEncoder->APIDispatch(copySize.width, copySize.height,
                                            copySize.depthOrArrayLayers);
                    break;
                case Aspect::Stencil:
                    passEncoder->APIDispatch((copySize.width + 3) / 4, copySize.height,
                                            copySize.depthOrArrayLayers);
                    break;
                default:
                    UNREACHABLE();
            }

            passEncoder->APIEndPass();

            CopyBufferToTextureCmd* copy =
                allocator->Allocate<CopyBufferToTextureCmd>(Command::CopyBufferToTexture);
            copy->source.buffer = resultBuffer.Get();
            copy->source.offset = 0;
            copy->source.bytesPerRow = bytesPerRow;
            copy->source.rowsPerImage = rowsPerImage;
            copy->destination.texture = destination->texture;
            copy->destination.origin = destination->origin;
            copy->destination.mipLevel = destination->mipLevel;
            copy->destination.aspect = aspect;
            copy->copySize = copySize;

        }

        return {};
    }

}  // namespace dawn_native
