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

#ifndef DAWNNATIVE_RENDERBUNDLE_H_
#define DAWNNATIVE_RENDERBUNDLE_H_

#include "common/Constants.h"
#include "dawn_native/CommandAllocator.h"
#include "dawn_native/Error.h"
#include "dawn_native/ObjectBase.h"

#include "dawn_native/dawn_platform.h"

#include <bitset>

namespace dawn_native {

    struct BeginRenderPassCmd;
    struct RenderBundleDescriptor;
    class RenderBundleEncoderBase;

    struct RenderBundleAttachmentInfo {
        std::bitset<kMaxColorAttachments> colorFormatsSet;
        dawn::TextureFormat colorFormats[kMaxColorAttachments];
        bool hasDepthStencilFormat;
        dawn::TextureFormat depthStencilFormat;
        uint32_t sampleCount;
    };

    class RenderBundleBase : public ObjectBase {
      public:
        RenderBundleBase(RenderBundleEncoderBase* encoder,
                         const RenderBundleDescriptor* descriptor,
                         const RenderBundleAttachmentInfo& attachmentInfo);
        ~RenderBundleBase() override;

        static RenderBundleBase* MakeError(DeviceBase* device);

        CommandIterator* GetCommands();

        MaybeError ValidateCompatibleWith(const BeginRenderPassCmd* renderPass) const;

      private:
        RenderBundleBase(DeviceBase* device, ErrorTag errorTag);

        CommandIterator mCommands;
        RenderBundleAttachmentInfo mAttachmentInfo;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_RENDERBUNDLE_H_
