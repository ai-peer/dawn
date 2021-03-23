// Copyright 2021 The Dawn Authors
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

#include "dawn_native/ExternalTexture.h"

#include "dawn_native/Device.h"
#include "dawn_native/Texture.h"

#include "dawn_native/dawn_platform.h"

namespace dawn_native {
    MaybeError ValidateExternalTextureDescriptor(const DeviceBase* device,
                                                 const ExternalTextureDescriptor* descriptor) {
        if (!descriptor) {
            return DAWN_VALIDATION_ERROR("The external texture descriptor is null.");
        }

        if (!descriptor->externalTexturePlane0) {
            return DAWN_VALIDATION_ERROR(
                "The external texture descriptor contains a null texture view.");
        }

        if (descriptor->externalTexturePlane0->GetTexture()->GetTextureState() ==
            TextureBase::TextureState::Destroyed) {
            return DAWN_VALIDATION_ERROR(
                "The external texture descriptor contains a texture view created from a destroyed "
                "texture.");
        }

        if (descriptor->externalTexturePlane0->GetTexture()->GetTextureState() !=
            TextureBase::TextureState::OwnedExternal) {
            return DAWN_VALIDATION_ERROR(
                "The external texture descriptor contains a texture view created from a internal "
                "texture.");
        }

        return {};
    }

    ExternalTextureBase::ExternalTextureBase(DeviceBase* device,
                                             const ExternalTextureDescriptor* descriptor)
        : ObjectBase(device) {
        textureViews.push_back(descriptor->externalTexturePlane0);
    }

    ExternalTextureBase::ExternalTextureBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : ObjectBase(device, tag) {
    }

    std::vector<Ref<TextureViewBase>> ExternalTextureBase::GetTextureViews() const {
        return textureViews;
    }

    void ExternalTextureBase::APIDestroy() {
        if (GetDevice()->ConsumedError(GetDevice()->ValidateObject(this))) {
            return;
        }
        ASSERT(!IsError());
    }

    // static
    ExternalTextureBase* ExternalTextureBase::MakeError(DeviceBase* device) {
        return new ExternalTextureBase(device, ObjectBase::kError);
    }

}  // namespace dawn_native