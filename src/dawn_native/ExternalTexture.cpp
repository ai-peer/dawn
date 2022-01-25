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

#include "dawn_native/Buffer.h"
#include "dawn_native/Device.h"
#include "dawn_native/ObjectType_autogen.h"
#include "dawn_native/Queue.h"
#include "dawn_native/Texture.h"

#include "dawn_native/dawn_platform.h"

namespace dawn::native {

    MaybeError ValidateExternalTexturePlane(const TextureViewBase* textureView) {
        DAWN_INVALID_IF(
            (textureView->GetTexture()->GetUsage() & wgpu::TextureUsage::TextureBinding) == 0,
            "The external texture plane (%s) usage (%s) doesn't include the required usage (%s)",
            textureView, textureView->GetTexture()->GetUsage(), wgpu::TextureUsage::TextureBinding);

        DAWN_INVALID_IF(textureView->GetDimension() != wgpu::TextureViewDimension::e2D,
                        "The external texture plane (%s) dimension (%s) is not 2D.", textureView,
                        textureView->GetDimension());

        DAWN_INVALID_IF(textureView->GetLevelCount() > 1,
                        "The external texture plane (%s) mip level count (%u) is not 1.",
                        textureView, textureView->GetLevelCount());

        DAWN_INVALID_IF(textureView->GetTexture()->GetSampleCount() != 1,
                        "The external texture plane (%s) sample count (%u) is not one.",
                        textureView, textureView->GetTexture()->GetSampleCount());

        return {};
    }

    MaybeError ValidateExternalTextureDescriptor(const DeviceBase* device,
                                                 const ExternalTextureDescriptor* descriptor) {
        ASSERT(descriptor);
        ASSERT(descriptor->plane0);

        DAWN_TRY(device->ValidateObject(descriptor->plane0));

        return {};
    }

    // static
    ResultOrError<Ref<ExternalTextureBase>> ExternalTextureBase::Create(
        DeviceBase* device,
        const ExternalTextureDescriptor* descriptor) {
        Ref<ExternalTextureBase> externalTexture =
            AcquireRef(new ExternalTextureBase(device, descriptor));
        DAWN_TRY(externalTexture->Initialize(device, descriptor));
        return std::move(externalTexture);
    }

    ExternalTextureBase::ExternalTextureBase(DeviceBase* device,
                                             const ExternalTextureDescriptor* descriptor)
        : ApiObjectBase(device, descriptor->label), mState(ExternalTextureState::Alive) {
        TrackInDevice();
    }

    ExternalTextureBase::ExternalTextureBase(DeviceBase* device)
        : ApiObjectBase(device, kLabelNotImplemented), mState(ExternalTextureState::Alive) {
        TrackInDevice();
    }

    ExternalTextureBase::ExternalTextureBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : ApiObjectBase(device, tag) {
    }

    MaybeError ExternalTextureBase::Initialize(DeviceBase* device,
                                               const ExternalTextureDescriptor* descriptor) {
        // Store any passed in TextureViews associated with individual planes.
        mTextureViews[0] = descriptor->plane0;
        mTextureViews[1] = descriptor->plane1;

        // We must create a buffer to store parameters needed by a shader that operates on this
        // external texture.
        BufferDescriptor bufferDesc;
        bufferDesc.size = sizeof(ExternalTextureParams);
        bufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        bufferDesc.label = "Dawn_External_Texture_Params_Buffer";

        DAWN_TRY_ASSIGN(mParamsBuffer, device->CreateBuffer(&bufferDesc));

        // Dawn & Tint's YUV to RGB conversion implementation was inspired by the conversions found
        // in libYUV. If this implementation needs expanded to support more colorspaces, this file
        // is an excellent reference: chromium/src/third_party/libyuv/source/row_common.cc.
        //
        // The conversion from YUV to RGB looks like this:
        // r = Y * 1.164          + V * vr
        // g = Y * 1.164 - U * ug - V * vg
        // b = Y * 1.164 + U * ub
        //
        // By changing the values of vr, vg, ub, and ug we can change the destination color space.
        ExternalTextureParams params;
        params.numPlanes = descriptor->plane1 == nullptr ? 1 : 2;

        switch (descriptor->colorSpace) {
            case wgpu::PredefinedColorSpace::Srgb:
                // Numbers derived from ITU-R recommendation for limited range BT.709
                params.vr = 1.793;
                params.vg = 0.392;
                params.ub = 0.813;
                params.ug = 2.017;
                break;
            case wgpu::PredefinedColorSpace::Undefined:
                break;
        }

        DAWN_TRY(device->GetQueue()->WriteBuffer(mParamsBuffer.Get(), 0, &params,
                                                 sizeof(ExternalTextureParams)));

        return {};
    }

    const std::array<Ref<TextureViewBase>, kMaxPlanesPerFormat>&
    ExternalTextureBase::GetTextureViews() const {
        return mTextureViews;
    }

    MaybeError ExternalTextureBase::ValidateCanUseInSubmitNow() const {
        ASSERT(!IsError());
        DAWN_INVALID_IF(mState == ExternalTextureState::Destroyed,
                        "Destroyed external texture %s is used in a submit.", this);
        return {};
    }

    void ExternalTextureBase::APIDestroy() {
        if (GetDevice()->ConsumedError(GetDevice()->ValidateObject(this))) {
            return;
        }
        Destroy();
    }

    void ExternalTextureBase::DestroyImpl() {
        mState = ExternalTextureState::Destroyed;
    }

    // static
    ExternalTextureBase* ExternalTextureBase::MakeError(DeviceBase* device) {
        return new ExternalTextureBase(device, ObjectBase::kError);
    }

    Ref<BufferBase> ExternalTextureBase::GetParamsBuffer() const {
        return mParamsBuffer;
    }

    ObjectType ExternalTextureBase::GetType() const {
        return ObjectType::ExternalTexture;
    }

}  // namespace dawn::native
