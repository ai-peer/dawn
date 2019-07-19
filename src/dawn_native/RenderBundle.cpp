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

#include "dawn_native/RenderBundle.h"

#include "common/BitSetIterator.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"
#include "dawn_native/RenderBundleEncoder.h"

namespace dawn_native {

    RenderBundleBase::RenderBundleBase(RenderBundleEncoderBase* encoder,
                                       const RenderBundleDescriptor* descriptor,
                                       const RenderBundleAttachmentInfo& attachmentInfo)
        : ObjectBase(encoder->GetDevice()),
          mCommands(encoder->AcquireCommands()),
          mAttachmentInfo(attachmentInfo) {
    }

    RenderBundleBase::~RenderBundleBase() {
        FreeCommands(&mCommands);
    }

    // static
    RenderBundleBase* RenderBundleBase::MakeError(DeviceBase* device) {
        return new RenderBundleBase(device, ObjectBase::kError);
    }

    RenderBundleBase::RenderBundleBase(DeviceBase* device, ErrorTag errorTag)
        : ObjectBase(device, errorTag) {
    }

    CommandIterator* RenderBundleBase::GetCommands() {
        return &mCommands;
    }

    MaybeError RenderBundleBase::ValidateCompatibleWith(
        const BeginRenderPassCmd* renderPass) const {
        ASSERT(renderPass != nullptr);
        ASSERT(!IsError());

        if (mAttachmentInfo.colorFormatsSet != renderPass->colorAttachmentsSet) {
            return DAWN_VALIDATION_ERROR(
                "Render bundle doesn't have same color attachments set as renderPass");
        }

        for (uint32_t i : IterateBitSet(mAttachmentInfo.colorFormatsSet)) {
            if (renderPass->colorAttachments[i].view->GetFormat().format !=
                mAttachmentInfo.colorFormats[i]) {
                return DAWN_VALIDATION_ERROR("Render bundle color format doesn't match renderPass");
            }
        }

        if (renderPass->hasDepthStencilAttachment != mAttachmentInfo.hasDepthStencilFormat) {
            return DAWN_VALIDATION_ERROR(
                "Render bundle depth stencil format doesn't match renderPass");
        }

        if (renderPass->hasDepthStencilAttachment &&
            renderPass->depthStencilAttachment.view->GetFormat().format !=
                mAttachmentInfo.depthStencilFormat) {
            return DAWN_VALIDATION_ERROR(
                "Render bundle depth stencil format doesn't match renderPass");
        }

        if (renderPass->sampleCount != mAttachmentInfo.sampleCount) {
            return DAWN_VALIDATION_ERROR("Render bundle sample count doesn't match renderPass");
        }

        return {};
    }

}  // namespace dawn_native
