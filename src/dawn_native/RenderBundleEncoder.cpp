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
#include "dawn_native/ValidationUtils_autogen.h"

namespace dawn_native {

    MaybeError ValidateRenderBundleEncoderDescriptor(
        const DeviceBase* device,
        const RenderBundleEncoderDescriptor* descriptor) {
        if (!IsValidSampleCount(descriptor->sampleCount)) {
            return DAWN_VALIDATION_ERROR("Sample count is not supported");
        }

        if (descriptor->colorFormatsCount > kMaxColorAttachments) {
            return DAWN_VALIDATION_ERROR("Color formats count exceeds maximum");
        }

        if (descriptor->colorFormatsCount == 0 && !descriptor->depthStencilFormat) {
            return DAWN_VALIDATION_ERROR("Should have at least one texture format");
        }

        for (uint32_t i = 0; i < descriptor->colorFormatsCount; ++i) {
            DAWN_TRY(ValidateTextureFormat(descriptor->colorFormats[i]));
        }

        if (descriptor->depthStencilFormat != nullptr) {
            DAWN_TRY(ValidateTextureFormat(*descriptor->depthStencilFormat));
        }

        return {};
    }

    RenderBundleEncoderBase::RenderBundleEncoderBase(
        DeviceBase* device,
        const RenderBundleEncoderDescriptor* descriptor)
        : RenderEncoderBase(device, &mEncodingContext), mEncodingContext(device, this) {
        ASSERT(descriptor != nullptr);

        mAttachmentInfo.hasDepthStencilFormat = descriptor->depthStencilFormat != nullptr;
        if (mAttachmentInfo.hasDepthStencilFormat) {
            mAttachmentInfo.depthStencilFormat = *descriptor->depthStencilFormat;
        }

        for (uint32_t i = 0; i < descriptor->colorFormatsCount; ++i) {
            mAttachmentInfo.colorFormatsSet.set(i);
            mAttachmentInfo.colorFormats[i] = descriptor->colorFormats[i];
        }

        mAttachmentInfo.sampleCount = descriptor->sampleCount;
    }

    RenderBundleEncoderBase::RenderBundleEncoderBase(DeviceBase* device, ErrorTag errorTag)
        : RenderEncoderBase(device, &mEncodingContext, errorTag), mEncodingContext(device, this) {
    }

    // static
    RenderBundleEncoderBase* RenderBundleEncoderBase::MakeError(DeviceBase* device) {
        return new RenderBundleEncoderBase(device, ObjectBase::kError);
    }

    CommandIterator RenderBundleEncoderBase::AcquireCommands() {
        return mEncodingContext.AcquireCommands();
    }

    RenderBundleBase* RenderBundleEncoderBase::Finish(const RenderBundleDescriptor* descriptor) {
        if (GetDevice()->ConsumedError(ValidateFinish(descriptor))) {
            return RenderBundleBase::MakeError(GetDevice());
        }
        ASSERT(!IsError());

        return new RenderBundleBase(this, descriptor, mAttachmentInfo);
    }

    MaybeError RenderBundleEncoderBase::ValidateFinish(const RenderBundleDescriptor* descriptor) {
        DAWN_TRY(GetDevice()->ValidateObject(this));

        // Even if Finish() validation fails, calling it will mutate the internal state of the
        // encoding context. Subsequent calls to encode commands will generate errors.
        DAWN_TRY(mEncodingContext.Finish());

        CommandIterator* commands = mEncodingContext.GetIterator();
        Command type;
        while (commands->NextCommandId(&type)) {
            switch (type) {
                case Command::SetRenderPipeline: {
                    SetRenderPipelineCmd* cmd = commands->NextCommand<SetRenderPipelineCmd>();
                    RenderPipelineBase* pipeline = cmd->pipeline.Get();

                    DAWN_TRY(pipeline->ValidateCompatibleWith(&mAttachmentInfo));
                } break;

                case Command::Draw:
                case Command::DrawIndexed:
                case Command::DrawIndirect:
                case Command::DrawIndexedIndirect:
                case Command::InsertDebugMarker:
                case Command::PopDebugGroup:
                case Command::PushDebugGroup:
                case Command::SetBindGroup:
                case Command::SetIndexBuffer:
                case Command::SetVertexBuffers:
                    SkipCommand(commands, type);
                    break;

                default:
                    return DAWN_VALIDATION_ERROR("Command disallowed inside a render bundle");
            }
        }

        return {};
    }

}  // namespace dawn_native
