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

#include "dawn_native/BlendState.h"

#include "dawn_native/Device.h"
#include "dawn_native/ValidationUtils_autogen.h"

namespace dawn_native {

    MaybeError ValidateBlendStateDescriptor(DeviceBase* device,
                                            const BlendStateDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }
        DAWN_TRY(ValidateBlendOperation(descriptor->alphaBlend.operation));
        DAWN_TRY(ValidateBlendFactor(descriptor->alphaBlend.srcFactor));
        DAWN_TRY(ValidateBlendFactor(descriptor->alphaBlend.dstFactor));
        DAWN_TRY(ValidateBlendOperation(descriptor->colorBlend.operation));
        DAWN_TRY(ValidateBlendFactor(descriptor->colorBlend.srcFactor));
        DAWN_TRY(ValidateBlendFactor(descriptor->colorBlend.dstFactor));
        DAWN_TRY(ValidateColorWriteMask(descriptor->colorWriteMask));
        return {};
    }

    BlendStateBase::BlendStateBase(DeviceBase* device, const BlendStateDescriptor* descriptor)
        : ObjectBase(device), mDescriptor(*descriptor) {
    }

    const BlendStateDescriptor* BlendStateBase::GetBlendStateDescriptor() const {
        return &mDescriptor;
    }

}  // namespace dawn_native
