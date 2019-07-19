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
        : RenderEncoderBase(device, nullptr), mEncodingContext(device, this) {
        ASSERT(descriptor != nullptr);
        mAttachmentInfo.colorFormatsCount = descriptor->colorFormatsCount;
        mAttachmentInfo.sampleCount = descriptor->sampleCount;
        for (uint32_t i = 0; i < descriptor->colorFormatsCount; ++i) {
            mAttachmentInfo.colorFormats[i] = descriptor->colorFormats[i];
        }
    }

    CommandIterator RenderBundleEncoderBase::AcquireCommands() {
        return mEncodingContext.AcquireCommands();
    }

    void RenderBundleEncoderBase::SetPipeline(RenderPipelineBase* pipeline) {
        // This is the only command we can validate in the bundle, and validation only
        // involves checking matching texture formats. We validate it right away instead of
        // walking the command buffer on Finish().
        if (!pipeline->IsError()) {
            mEncodingContext.TryEncode(this, [&](CommandAllocator*) -> MaybeError {
                return pipeline->ValidateCompatibleWith(&mAttachmentInfo);
            });
        }
        RenderEncoderBase::SetPipeline(pipeline);
    }

    RenderBundleBase* RenderBundleEncoderBase::Finish(const RenderBundleDescriptor* descriptor) {
        if (GetDevice()->ConsumedError(ValidateFinish(descriptor))) {
            return RenderBundleBase::MakeError(GetDevice());
        }
        ASSERT(!IsError());

        mEncodingContext.GetIterator();
        return new RenderBundleBase(this, descriptor, mAttachmentInfo);
    }

    MaybeError RenderBundleEncoderBase::ValidateFinish(const RenderBundleDescriptor* descriptor) {
        DAWN_TRY(GetDevice()->ValidateObject(this));

        // Even if Finish() validation fails, calling it will mutate the internal state of the
        // encoding context. Subsequent calls to encode commands will generate errors.
        DAWN_TRY(mEncodingContext.Finish());

        return {};
    }

}  // namespace dawn_native
