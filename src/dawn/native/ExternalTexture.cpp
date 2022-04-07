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

#include "dawn/native/ExternalTexture.h"

#include "dawn/native/Buffer.h"
#include "dawn/native/Device.h"
#include "dawn/native/ObjectType_autogen.h"
#include "dawn/native/Queue.h"
#include "dawn/native/Texture.h"

#include "dawn/native/dawn_platform.h"

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

        wgpu::TextureFormat plane0Format = descriptor->plane0->GetFormat().format;

        if (descriptor->plane1) {
            DAWN_INVALID_IF(
                device->IsToggleEnabled(Toggle::DisallowUnsafeAPIs),
                "Bi-planar external textures are disabled until the implementation is completed.");

            DAWN_INVALID_IF(descriptor->colorSpace != wgpu::PredefinedColorSpace::Srgb,
                            "The specified destination color space (%s) is not %s.",
                            descriptor->colorSpace, wgpu::PredefinedColorSpace::Srgb);

            DAWN_TRY(device->ValidateObject(descriptor->plane1));
            wgpu::TextureFormat plane1Format = descriptor->plane1->GetFormat().format;

            DAWN_INVALID_IF(plane0Format != wgpu::TextureFormat::R8Unorm,
                            "The bi-planar external texture plane (%s) format (%s) is not %s.",
                            descriptor->plane0, plane0Format, wgpu::TextureFormat::R8Unorm);
            DAWN_INVALID_IF(plane1Format != wgpu::TextureFormat::RG8Unorm,
                            "The bi-planar external texture plane (%s) format (%s) is not %s.",
                            descriptor->plane1, plane1Format, wgpu::TextureFormat::RG8Unorm);

            DAWN_TRY(ValidateExternalTexturePlane(descriptor->plane0));
            DAWN_TRY(ValidateExternalTexturePlane(descriptor->plane1));
        } else {
            switch (plane0Format) {
                case wgpu::TextureFormat::RGBA8Unorm:
                case wgpu::TextureFormat::BGRA8Unorm:
                case wgpu::TextureFormat::RGBA16Float:
                    DAWN_TRY(ValidateExternalTexturePlane(descriptor->plane0));
                    break;
                default:
                    return DAWN_FORMAT_VALIDATION_ERROR(
                        "The external texture plane (%s) format (%s) is not a supported format "
                        "(%s, %s, %s).",
                        descriptor->plane0, plane0Format, wgpu::TextureFormat::RGBA8Unorm,
                        wgpu::TextureFormat::BGRA8Unorm, wgpu::TextureFormat::RGBA16Float);
            }
        }

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

    ExternalTextureBase::~ExternalTextureBase() = default;

    MaybeError ExternalTextureBase::Initialize(DeviceBase* device,
                                               const ExternalTextureDescriptor* descriptor) {
        // Store any passed in TextureViews associated with individual planes.
        mTextureViews[0] = descriptor->plane0;

        if (descriptor->plane1) {
            mTextureViews[1] = descriptor->plane1;
        } else {
            DAWN_TRY_ASSIGN(mTextureViews[1],
                            device->GetOrCreateDummyTextureViewForExternalTexture());
        }

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
        // y = Y - yRangeOffset
        // u = U - uvRangeOffset
        // v = V - uvRangeOffset
        // r = y * yRangeMultiplier + v * vr
        // g = y * yRangeMultiplier - u * ug - v * vg
        // b = y * yRangeMultiplier + u * ub
        //
        // By changing the parameter values we can use the correct YUV to RGB conversion to match
        // the source video color space.
        ExternalTextureParams params;
        params.numPlanes = descriptor->plane1 == nullptr ? 1 : 2;

        switch (descriptor->videoSourceColorSpace) {
            case wgpu::VideoSourceColorSpace::Bt601Limited:
                params.vr = 1.596;
                params.ug = 0.391;
                params.vg = 0.813;
                params.ub = 2.018;
                params.yRangeMultiplier = 1.164;
                params.yRangeOffset = 0.0625;
                params.uvRangeOffset = 0.5;
                break;
            case wgpu::VideoSourceColorSpace::Bt601Full:
                params.vr = 1.40200;
                params.ug = 0.34414;
                params.vg = 0.71414;
                params.ub = 1.77200;
                params.yRangeMultiplier = 1.0;
                params.yRangeOffset = 0.0;
                params.uvRangeOffset = 0.0;
                break;
            case wgpu::VideoSourceColorSpace::Bt709Limited:
                params.vr = 1.793;
                params.ug = 0.213;
                params.vg = 0.533;
                params.ub = 2.112;
                params.yRangeMultiplier = 1.164;
                params.yRangeOffset = 0.0625;
                params.uvRangeOffset = 0.5;
                break;
            case wgpu::VideoSourceColorSpace::Bt709Full:
                params.vr = 1.5748;
                params.ug = 0.18732;
                params.vg = 0.46812;
                params.ub = 1.8556;
                params.yRangeMultiplier = 1.0;
                params.yRangeOffset = 0.0;
                params.uvRangeOffset = 0.0;
                break;
            case wgpu::VideoSourceColorSpace::Bt2020Limited:
                params.vr = 1.67867;
                params.ug = 0.187326;
                params.vg = 0.65042;
                params.ub = 2.14177;
                params.yRangeMultiplier = 1.164384;
                params.yRangeOffset = 0.0625;
                params.uvRangeOffset = 0.5;
                break;
            default:
                UNREACHABLE();
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

    BufferBase* ExternalTextureBase::GetParamsBuffer() const {
        return mParamsBuffer.Get();
    }

    ObjectType ExternalTextureBase::GetType() const {
        return ObjectType::ExternalTexture;
    }

}  // namespace dawn::native
