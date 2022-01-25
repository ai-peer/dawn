// Copyright 2022 The Dawn Authors
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

#include "dawn_native/metal/ExternalTextureMTL.h"
#include "dawn_native/Device.h"

namespace dawn::native::metal {

    // static
    ResultOrError<Ref<ExternalTexture>> ExternalTexture::Create(
        DeviceBase* device,
        const ExternalTextureDescriptor* descriptor) {
        Ref<ExternalTexture> externalTexture = AcquireRef(new ExternalTexture(device, descriptor));
        DAWN_TRY(externalTexture->Initialize(device, descriptor));
        return std::move(externalTexture);
    }

    MaybeError ExternalTexture::Initialize(DeviceBase* device,
                                           const ExternalTextureDescriptor* descriptor) {
        DAWN_TRY(ExternalTextureBase::Initialize(device, descriptor));

        if (descriptor->plane1 == nullptr) {
            TextureDescriptor textureDesc;
            textureDesc.dimension = wgpu::TextureDimension::e2D;
            textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
            textureDesc.label = "Dawn_External_Texture_Dummy_Texture";
            textureDesc.size = {1, 1, 1};
            textureDesc.usage = wgpu::TextureUsage::TextureBinding;

            DAWN_TRY_ASSIGN(mDummyTexture, device->CreateTexture(&textureDesc));

            TextureViewDescriptor textureViewDesc;
            textureViewDesc.arrayLayerCount = 1;
            textureViewDesc.aspect = wgpu::TextureAspect::All;
            textureViewDesc.baseArrayLayer = 0;
            textureViewDesc.dimension = wgpu::TextureViewDimension::e2D;
            textureViewDesc.format = wgpu::TextureFormat::RGBA8Unorm;
            textureViewDesc.label = "Dawn_External_Texture_Dummy_Texture_View";
            textureViewDesc.mipLevelCount = 1;

            DAWN_TRY_ASSIGN(mTextureViews[1],
                            device->CreateTextureView(mDummyTexture.Get(), &textureViewDesc));
        }

        return {};
    }

}  // namespace dawn::native::metal