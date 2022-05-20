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

#include "dawn/native/d3d12/CacheKeyD3D12.h"
#include "dawn/common/Constants.h"

namespace dawn::native {

template <>
void CacheKeySerializer<D3D12_COMPUTE_PIPELINE_STATE_DESC>::Serialize(
    CacheKey* key,
    const D3D12_COMPUTE_PIPELINE_STATE_DESC& t) {
    key->Record(t.CS);
    // TODO:
}

template <>
void CacheKeySerializer<D3D12_RENDER_TARGET_BLEND_DESC>::Serialize(
    CacheKey* key,
    const D3D12_RENDER_TARGET_BLEND_DESC& t) {
    key->Record(t.BlendEnable, t.LogicOpEnable, t.SrcBlend, t.DestBlend, t.BlendOp, t.SrcBlendAlpha,
                t.DestBlendAlpha, t.BlendOpAlpha, t.LogicOp, t.RenderTargetWriteMask);
}

template <>
void CacheKeySerializer<D3D12_BLEND_DESC>::Serialize(CacheKey* key, const D3D12_BLEND_DESC& t) {
    key->Record(t.AlphaToCoverageEnable, t.IndependentBlendEnable)
        .RecordIterable(t.RenderTarget,
                        // 8
                        kMaxColorAttachments);
}

template <>
void CacheKeySerializer<D3D12_DEPTH_STENCILOP_DESC>::Serialize(
    CacheKey* key,
    const D3D12_DEPTH_STENCILOP_DESC& t) {
    key->Record(t.StencilFailOp, t.StencilDepthFailOp, t.StencilPassOp, t.StencilFunc);
}

template <>
void CacheKeySerializer<D3D12_DEPTH_STENCIL_DESC>::Serialize(CacheKey* key,
                                                             const D3D12_DEPTH_STENCIL_DESC& t) {
    key->Record(t.DepthEnable, t.DepthWriteMask, t.DepthFunc, t.StencilEnable, t.StencilReadMask,
                t.StencilWriteMask, t.FrontFace, t.BackFace);
}

template <>
void CacheKeySerializer<D3D12_RASTERIZER_DESC>::Serialize(CacheKey* key,
                                                          const D3D12_RASTERIZER_DESC& t) {
    key->Record(t.FillMode, t.CullMode, t.FrontCounterClockwise, t.DepthBias, t.DepthBiasClamp,
                t.SlopeScaledDepthBias, t.DepthClipEnable, t.MultisampleEnable,
                t.AntialiasedLineEnable, t.ForcedSampleCount, t.ConservativeRaster);
}

template <>
void CacheKeySerializer<DXGI_SAMPLE_DESC>::Serialize(CacheKey* key, const DXGI_SAMPLE_DESC& t) {
    key->Record(t.Count, t.Quality);
}

template <>
void CacheKeySerializer<D3D12_SHADER_BYTECODE>::Serialize(CacheKey* key,
                                                          const D3D12_SHADER_BYTECODE& t) {
    key->RecordIterable(reinterpret_cast<const uint8_t*>(t.pShaderBytecode), t.BytecodeLength);
}

template <>
void CacheKeySerializer<D3D12_GRAPHICS_PIPELINE_STATE_DESC>::Serialize(
    CacheKey* key,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC& t) {
    // Don't record shader byte code here, but record in RenderPipelineD3D12.cpp when iterate stages
    key->Record(t.BlendState)
        .Record(t.SampleMask)
        .Record(t.PrimitiveTopologyType)
        .Record(t.SampleDesc)
        .Record(t.RasterizerState)
        .Record(t.DepthStencilState)
        .Record(t.NumRenderTargets)
        // TODO: record all fields in descriptor
        ;
}

template <>
void CacheKeySerializer<ComPtr<ID3DBlob>>::Serialize(CacheKey* key, const ComPtr<ID3DBlob>& t) {
    // key->RecordIterable<char>(t->GetBufferPointer(), t->GetBufferSize());
    key->RecordIterable(reinterpret_cast<uint8_t*>(t->GetBufferPointer()), t->GetBufferSize());
}

template <>
void CacheKeySerializer<D3D12_DESCRIPTOR_RANGE>::Serialize(CacheKey* key,
                                                           const D3D12_DESCRIPTOR_RANGE& t) {
    // key->Record(t.BaseShaderRegister, t.RangeType, t.NumDescriptors, t.RegisterSpace);
    key->Record(t.RangeType, t.NumDescriptors, t.BaseShaderRegister, t.RegisterSpace,
                t.OffsetInDescriptorsFromTableStart);
}

template <>
void CacheKeySerializer<std::vector<D3D12_DESCRIPTOR_RANGE>>::Serialize(
    CacheKey* key,
    const std::vector<D3D12_DESCRIPTOR_RANGE>& t) {
    key->RecordIterable(t.data(), t.size());
    // vulkan::SerializePnext<>(key, &t);
}

}  // namespace dawn::native