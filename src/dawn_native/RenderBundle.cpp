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

#include "dawn_native/Device.h"
#include "dawn_native/RenderBundleEncoder.h"

#include "dawn_native/dawn_platform.h"

namespace dawn_native {

    RenderBundleBase::RenderBundleBase(RenderBundleEncoderBase* encoder,
                                       const RenderBundleDescriptor* descriptor,
                                       const RenderBundleAttachmentInfo& attachmentInfo)
        : ObjectBase(encoder->GetDevice()),
          mCommands(encoder->AcquireCommands()),
          mAttachmentInfo(attachmentInfo) {
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

    const RenderBundleAttachmentInfo& RenderBundleBase::GetAttachmentInfo() const {
        return mAttachmentInfo;
    }

    MaybeError RenderBundleBase::ValidateCompatibleWith(
        const BeginRenderPassCmd* renderPass) const {
        ASSERT(!IsError());
        // TODO
        return {};
    }

}  // namespace dawn_native
