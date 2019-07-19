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

#include "dawn_native/RenderBundleEncoder.h"

#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"
#include "dawn_native/RenderPipeline.h"

namespace dawn_native {

    RenderBundleEncoderBase::RenderBundleEncoderBase(
        DeviceBase* device,
        const RenderBundleEncoderDescriptor* descriptor)
        : CommandRecorder(device), RenderEncoderBase(device, this, &mCommandAllocator) {
        ASSERT(descriptor != nullptr);
        mAttachmentInfo.colorFormatsCount = descriptor->colorFormatsCount;
        mAttachmentInfo.sampleCount = descriptor->sampleCount;
        for (uint32_t i = 0; i < descriptor->colorFormatsCount; ++i) {
            mAttachmentInfo.colorFormats[i] = descriptor->colorFormats[i];
        }
    }

    void RenderBundleEncoderBase::SetPipeline(RenderPipelineBase* pipeline) {
        // This is the only command we can validate in the bundle, and validation only
        // involves checking matching texture formats. We validate it right away instead of
        // walking the command buffer on Finish().
        if (!pipeline->IsError()) {
            ConsumedError(pipeline->ValidateCompatibleWith(&mAttachmentInfo));
        }
        RenderEncoderBase::SetPipeline(pipeline);
    }

    RenderBundleBase* RenderBundleEncoderBase::Finish(const RenderBundleDescriptor* descriptor) {
        // Even if finish validation fails, it is now invalid to call any encoding commands on
        // this object, so we clear the allocator.
        mIsRecording = false;
        mAllocator = nullptr;

        if (GetDevice()->ConsumedError(ValidateFinish(descriptor))) {
            return RenderBundleBase::MakeError(GetDevice());
        }
        ASSERT(!IsError());

        MoveToIterator();
        return new RenderBundleBase(this, descriptor, mAttachmentInfo);
    }

    MaybeError RenderBundleEncoderBase::ValidateFinish(const RenderBundleDescriptor* descriptor) {
        DAWN_TRY(GetDevice()->ValidateObject(this));
        if (mGotError) {
            return DAWN_VALIDATION_ERROR(mErrorMessage);
        }
        return {};
    }

}  // namespace dawn_native
