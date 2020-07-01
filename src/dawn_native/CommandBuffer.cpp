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

#include "dawn_native/CommandBuffer.h"

#include "common/BitSetIterator.h"
#include "dawn_native/CommandEncoder.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Format.h"
#include "dawn_native/Texture.h"

namespace dawn_native {

    CommandBufferBase::CommandBufferBase(CommandEncoder* encoder, const CommandBufferDescriptor*)
        : ObjectBase(encoder->GetDevice()), mResourceUsages(encoder->AcquireResourceUsages()) {
    }

    CommandBufferBase::CommandBufferBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : ObjectBase(device, tag) {
    }

    // static
    CommandBufferBase* CommandBufferBase::MakeError(DeviceBase* device) {
        return new CommandBufferBase(device, ObjectBase::kError);
    }

    const CommandBufferResourceUsage& CommandBufferBase::GetResourceUsages() const {
        return mResourceUsages;
    }

    bool IsCompleteSubresourceCopiedTo(const TextureBase* texture,
                                       const Extent3D copySize,
                                       const uint32_t mipLevel) {
        Extent3D extent = texture->GetMipLevelPhysicalSize(mipLevel);

        ASSERT(texture->GetDimension() == wgpu::TextureDimension::e2D);
        if (extent.width == copySize.width && extent.height == copySize.height) {
            return true;
        }
        return false;
    }

    SubresourceRange GetSubresourcesAffectedByCopy(const TextureCopy& copy,
                                                   const Extent3D& copySize) {
        switch (copy.texture->GetDimension()) {
            case wgpu::TextureDimension::e2D:
                // TODO(enga): This should use the copy aspect when we have it.
                return {copy.mipLevel, 1, copy.origin.z, copySize.depth,
                        copy.texture->GetFormat().aspectMask};
            default:
                UNREACHABLE();
                return {};
        }
    }

    void LazyClearRenderPassAttachments(BeginRenderPassCmd* renderPass) {
        for (uint32_t i : IterateBitSet(renderPass->attachmentState->GetColorAttachmentsMask())) {
            auto& attachmentInfo = renderPass->colorAttachments[i];
            TextureViewBase* view = attachmentInfo.view.Get();
            bool hasResolveTarget = attachmentInfo.resolveTarget.Get() != nullptr;

            ASSERT(view->GetLayerCount() == 1);
            ASSERT(view->GetLevelCount() == 1);
            SubresourceRange range = view->GetSubresourceRange();
            ASSERT(IsColor(range.aspectMask));

            // If the loadOp is Load, but the subresource is not initialized, use Clear instead.
            if (attachmentInfo.loadOp == wgpu::LoadOp::Load &&
                !view->GetTexture()->IsSubresourceContentInitialized(range)) {
                attachmentInfo.loadOp = wgpu::LoadOp::Clear;
                attachmentInfo.clearColor = {0.f, 0.f, 0.f, 0.f};
            }

            if (hasResolveTarget) {
                // We need to set the resolve target to initialized so that it does not get
                // cleared later in the pipeline. The texture will be resolved from the
                // source color attachment, which will be correctly initialized.
                TextureViewBase* resolveView = attachmentInfo.resolveTarget.Get();
                ASSERT(resolveView->GetLayerCount() == 1);
                ASSERT(resolveView->GetLevelCount() == 1);
                ASSERT(IsColor(resolveView->GetAspectMask()));
                resolveView->GetTexture()->SetIsSubresourceContentInitialized(
                    true, resolveView->GetSubresourceRange());
            }

            switch (attachmentInfo.storeOp) {
                case wgpu::StoreOp::Store:
                    view->GetTexture()->SetIsSubresourceContentInitialized(true, range);
                    break;

                case wgpu::StoreOp::Clear:
                    view->GetTexture()->SetIsSubresourceContentInitialized(false, range);
                    break;

                default:
                    UNREACHABLE();
                    break;
            }
        }

        if (renderPass->attachmentState->HasDepthStencilAttachment()) {
            auto& attachmentInfo = renderPass->depthStencilAttachment;
            TextureViewBase* view = attachmentInfo.view.Get();
            ASSERT(view->GetLayerCount() == 1);
            ASSERT(view->GetLevelCount() == 1);
            SubresourceRange range = view->GetSubresourceRange();

            for (TextureAspect aspect : IterateBitSet(range.aspectMask)) {
                SubresourceRange singleAspectRange = range;
                singleAspectRange.aspectMask = SingleAspect(aspect);
                bool loadInitialized =
                    view->GetTexture()->IsSubresourceContentInitialized(singleAspectRange);

                bool storeInitialized;
                switch (aspect) {
                    case TextureAspect::Depth:
                        // If the depth stencil texture has not been initialized, we want to use
                        // loadop clear to init the contents to 0's
                        if (!loadInitialized && attachmentInfo.depthLoadOp == wgpu::LoadOp::Load) {
                            attachmentInfo.clearDepth = 0.0f;
                            attachmentInfo.depthLoadOp = wgpu::LoadOp::Clear;
                        }
                        switch (attachmentInfo.depthStoreOp) {
                            case wgpu::StoreOp::Store:
                                storeInitialized = true;
                                break;

                            case wgpu::StoreOp::Clear:
                                storeInitialized = false;
                                break;

                            default:
                                UNREACHABLE();
                                break;
                        }
                        break;

                    case TextureAspect::Stencil:
                        // If the depth stencil texture has not been initialized, we want to use
                        // loadop clear to init the contents to 0's
                        if (!loadInitialized &&
                            attachmentInfo.stencilLoadOp == wgpu::LoadOp::Load) {
                            attachmentInfo.clearStencil = 0u;
                            attachmentInfo.stencilLoadOp = wgpu::LoadOp::Clear;
                        }

                        switch (attachmentInfo.stencilStoreOp) {
                            case wgpu::StoreOp::Store:
                                storeInitialized = true;
                                break;

                            case wgpu::StoreOp::Clear:
                                storeInitialized = false;
                                break;

                            default:
                                UNREACHABLE();
                                break;
                        }
                        break;

                    default:
                        UNREACHABLE();
                        break;
                }

                view->GetTexture()->SetIsSubresourceContentInitialized(storeInitialized,
                                                                       singleAspectRange);
            }
        }
    }

}  // namespace dawn_native
