// Copyright 2018 The Dawn Authors
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

#include "dawn_native/RenderPassEncoder.h"

#include "common/Constants.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandEncoder.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"
#include "dawn_native/RenderBundle.h"
#include "dawn_native/RenderPipeline.h"

#include <math.h>
#include <string.h>

namespace dawn_native {

    RenderPassEncoderBase::RenderPassEncoderBase(DeviceBase* device,
                                                 CommandEncoderBase* topLevelEncoder,
                                                 CommandAllocator* allocator)
        : RenderEncoderBase(device, topLevelEncoder, allocator), ObjectBase(device) {
    }

    RenderPassEncoderBase::RenderPassEncoderBase(DeviceBase* device,
                                                 CommandEncoderBase* topLevelEncoder,
                                                 ErrorTag errorTag)
        : RenderEncoderBase(device, topLevelEncoder, errorTag), ObjectBase(device, errorTag) {
    }

    RenderPassEncoderBase* RenderPassEncoderBase::MakeError(DeviceBase* device,
                                                            CommandEncoderBase* topLevelEncoder) {
        return new RenderPassEncoderBase(device, topLevelEncoder, ObjectBase::kError);
    }

    void RenderPassEncoderBase::EndPass() {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        reinterpret_cast<CommandEncoderBase*>(mCommandRecorder.Get())->PassEnded();
        mAllocator = nullptr;
    }

    void RenderPassEncoderBase::SetStencilReference(uint32_t reference) {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        SetStencilReferenceCmd* cmd =
            mAllocator->Allocate<SetStencilReferenceCmd>(Command::SetStencilReference);
        cmd->reference = reference;
    }

    void RenderPassEncoderBase::SetBlendColor(const Color* color) {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        SetBlendColorCmd* cmd = mAllocator->Allocate<SetBlendColorCmd>(Command::SetBlendColor);
        cmd->color = *color;
    }

    void RenderPassEncoderBase::SetViewport(float x,
                                            float y,
                                            float width,
                                            float height,
                                            float minDepth,
                                            float maxDepth) {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        if (isnan(x) || isnan(y) || isnan(width) || isnan(height) || isnan(minDepth) ||
            isnan(maxDepth)) {
            mCommandRecorder->HandleError("NaN is not allowed.");
            return;
        }

        // TODO(yunchao.he@intel.com): there are more restrictions for x, y, width and height in
        // Vulkan, and height can be a negative value in Vulkan 1.1. Revisit this part later (say,
        // for WebGPU v1).
        if (width <= 0 || height <= 0) {
            mCommandRecorder->HandleError("Width and height must be greater than 0.");
            return;
        }

        if (minDepth < 0 || minDepth > 1 || maxDepth < 0 || maxDepth > 1) {
            mCommandRecorder->HandleError("minDepth and maxDepth must be in [0, 1].");
            return;
        }

        SetViewportCmd* cmd = mAllocator->Allocate<SetViewportCmd>(Command::SetViewport);
        cmd->x = x;
        cmd->y = y;
        cmd->width = width;
        cmd->height = height;
        cmd->minDepth = minDepth;
        cmd->maxDepth = maxDepth;
    }

    void RenderPassEncoderBase::SetScissorRect(uint32_t x,
                                               uint32_t y,
                                               uint32_t width,
                                               uint32_t height) {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }
        if (width == 0 || height == 0) {
            mCommandRecorder->HandleError("Width and height must be greater than 0.");
            return;
        }

        SetScissorRectCmd* cmd = mAllocator->Allocate<SetScissorRectCmd>(Command::SetScissorRect);
        cmd->x = x;
        cmd->y = y;
        cmd->width = width;
        cmd->height = height;
    }

    void RenderPassEncoderBase::ExecuteBundles(uint32_t count,
                                               RenderBundleBase* const* renderBundles) {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        for (uint32_t i = 0; i < count; ++i) {
            if (mCommandRecorder->ConsumedError(GetDevice()->ValidateObject(renderBundles[i]))) {
                return;
            }
        }

        ExecuteBundlesCmd* cmd = mAllocator->Allocate<ExecuteBundlesCmd>(Command::ExecuteBundles);
        cmd->count = count;

        Ref<RenderBundleBase>* bundles = mAllocator->AllocateData<Ref<RenderBundleBase>>(count);
        for (uint32_t i = 0; i < count; ++i) {
            bundles[i] = renderBundles[i];
        }
    }

}  // namespace dawn_native
