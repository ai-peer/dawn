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

#include "dawn/native/d3d11/SamplerD3D11.h"

#include <algorithm>

#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/UtilsD3D11.h"

namespace dawn::native::d3d11 {

namespace {

D3D11_TEXTURE_ADDRESS_MODE D3D11TextureAddressMode(wgpu::AddressMode mode) {
    switch (mode) {
        case wgpu::AddressMode::Repeat:
            return D3D11_TEXTURE_ADDRESS_WRAP;
        case wgpu::AddressMode::MirrorRepeat:
            return D3D11_TEXTURE_ADDRESS_MIRROR;
        case wgpu::AddressMode::ClampToEdge:
            return D3D11_TEXTURE_ADDRESS_CLAMP;
    }
}

D3D11_FILTER_TYPE D3D11FilterType(wgpu::FilterMode mode) {
    switch (mode) {
        case wgpu::FilterMode::Nearest:
            return D3D11_FILTER_TYPE_POINT;
        case wgpu::FilterMode::Linear:
            return D3D11_FILTER_TYPE_LINEAR;
    }
}

}  // namespace

// static
Ref<Sampler> Sampler::Create(Device* device, const SamplerDescriptor* descriptor) {
    return AcquireRef(new Sampler(device, descriptor));
}

Sampler::Sampler(Device* device, const SamplerDescriptor* descriptor)
    : SamplerBase(device, descriptor) {
    D3D11_FILTER_TYPE minFilter = D3D11FilterType(descriptor->minFilter);
    D3D11_FILTER_TYPE magFilter = D3D11FilterType(descriptor->magFilter);
    D3D11_FILTER_TYPE mipmapFilter = D3D11FilterType(descriptor->mipmapFilter);

    D3D11_FILTER_REDUCTION_TYPE reduction = descriptor->compare == wgpu::CompareFunction::Undefined
                                                ? D3D11_FILTER_REDUCTION_TYPE_STANDARD
                                                : D3D11_FILTER_REDUCTION_TYPE_COMPARISON;

    // https://docs.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_sampler_desc
    mSamplerDesc.MaxAnisotropy = std::min<uint16_t>(GetMaxAnisotropy(), 16u);

    if (mSamplerDesc.MaxAnisotropy > 1) {
        mSamplerDesc.Filter = D3D11_ENCODE_ANISOTROPIC_FILTER(reduction);
    } else {
        mSamplerDesc.Filter =
            D3D11_ENCODE_BASIC_FILTER(minFilter, magFilter, mipmapFilter, reduction);
    }

    mSamplerDesc.AddressU = D3D11TextureAddressMode(descriptor->addressModeU);
    mSamplerDesc.AddressV = D3D11TextureAddressMode(descriptor->addressModeV);
    mSamplerDesc.AddressW = D3D11TextureAddressMode(descriptor->addressModeW);
    mSamplerDesc.MipLODBias = 0.f;

    if (descriptor->compare != wgpu::CompareFunction::Undefined) {
        mSamplerDesc.ComparisonFunc = ToD3D11ComparisonFunc(descriptor->compare);
    } else {
        // Still set the function so it's not garbage.
        mSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    }
    mSamplerDesc.MinLOD = descriptor->lodMinClamp;
    mSamplerDesc.MaxLOD = descriptor->lodMaxClamp;
}

const D3D11_SAMPLER_DESC& Sampler::GetSamplerDescriptor() const {
    return mSamplerDesc;
}

}  // namespace dawn::native::d3d11
