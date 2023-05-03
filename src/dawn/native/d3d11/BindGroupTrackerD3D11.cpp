// Copyright 2023 The Dawn Authors
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

#include "dawn/native/d3d11/BindGroupTrackerD3D11.h"

#include "dawn/common/Assert.h"
#include "dawn/native/Format.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d11/BindGroupD3D11.h"
#include "dawn/native/d3d11/BufferD3D11.h"
#include "dawn/native/d3d11/CommandRecordingContextD3D11.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/PipelineLayoutD3D11.h"
#include "dawn/native/d3d11/SamplerD3D11.h"
#include "dawn/native/d3d11/TextureD3D11.h"

namespace dawn::native::d3d11 {
namespace {

void UnsetSlots(CommandRecordingContext* commandContext,
                const PipelineLayout::PreStageSlots& usedSlots) {
    ID3D11DeviceContext1* deviceContext1 = commandContext->GetD3D11DeviceContext1();
    // Unset constant buffers
    for (UINT slot : IterateBitSet(usedSlots[wgpu::ShaderStage::Vertex].mConstantBufferSlots)) {
        ID3D11Buffer* nullBuffer = nullptr;
        deviceContext1->VSSetConstantBuffers1(slot, 1, &nullBuffer, nullptr, nullptr);
    }
    for (UINT slot : IterateBitSet(usedSlots[wgpu::ShaderStage::Fragment].mConstantBufferSlots)) {
        ID3D11Buffer* nullBuffer = nullptr;
        deviceContext1->PSSetConstantBuffers1(slot, 1, &nullBuffer, nullptr, nullptr);
    }
    for (UINT slot : IterateBitSet(usedSlots[wgpu::ShaderStage::Compute].mConstantBufferSlots)) {
        ID3D11Buffer* nullBuffer = nullptr;
        deviceContext1->CSSetConstantBuffers1(slot, 1, &nullBuffer, nullptr, nullptr);
    }

    // Unset shader resources
    for (UINT slot : IterateBitSet(usedSlots[wgpu::ShaderStage::Vertex].mInputResourceSlots)) {
        ID3D11ShaderResourceView* nullSRV = nullptr;
        deviceContext1->VSSetShaderResources(slot, 1, &nullSRV);
    }
    for (UINT slot : IterateBitSet(usedSlots[wgpu::ShaderStage::Fragment].mInputResourceSlots)) {
        ID3D11ShaderResourceView* nullSRV = nullptr;
        deviceContext1->PSSetShaderResources(slot, 1, &nullSRV);
    }
    for (UINT slot : IterateBitSet(usedSlots[wgpu::ShaderStage::Compute].mInputResourceSlots)) {
        ID3D11ShaderResourceView* nullSRV = nullptr;
        deviceContext1->CSSetShaderResources(slot, 1, &nullSRV);
    }

    // Unset samplers
    for (UINT slot : IterateBitSet(usedSlots[wgpu::ShaderStage::Vertex].mSamplerSlots)) {
        ID3D11SamplerState* nullSampler = nullptr;
        deviceContext1->VSSetSamplers(slot, 1, &nullSampler);
    }
    for (UINT slot : IterateBitSet(usedSlots[wgpu::ShaderStage::Fragment].mSamplerSlots)) {
        ID3D11SamplerState* nullSampler = nullptr;
        deviceContext1->PSSetSamplers(slot, 1, &nullSampler);
    }
    for (UINT slot : IterateBitSet(usedSlots[wgpu::ShaderStage::Compute].mSamplerSlots)) {
        ID3D11SamplerState* nullSampler = nullptr;
        deviceContext1->CSSetSamplers(slot, 1, &nullSampler);
    }

    // Unset unordered access views
    for (UINT slot : IterateBitSet(usedSlots[wgpu::ShaderStage::Compute].mUAVSlots)) {
        ID3D11UnorderedAccessView* nullUAV = nullptr;
        deviceContext1->CSSetUnorderedAccessViews(slot, 1, &nullUAV, nullptr);
    }
}

}  // namespace

BindGroupTracker::BindGroupTracker(CommandRecordingContext* commandContext)
    : mCommandContext(commandContext) {}

BindGroupTracker::~BindGroupTracker() {
    UnsetSlots(mCommandContext, mUsedSlots);
}

MaybeError BindGroupTracker::Apply() {
    BeforeApply();
    PipelineLayout::PreStageSlots usedSlots;
    for (BindGroupIndex index : IterateBitSet(mDirtyBindGroupsObjectChangedOrIsDynamic)) {
        GetGroupUsedSlots(index, &usedSlots);
    }

    // If setting an input resource will fail, if the resource is bind to the device context as
    // output, so we need to unset all affected slots before setting the new bind groups.
    UnsetSlots(mCommandContext, usedSlots);

    for (BindGroupIndex index : IterateBitSet(mDirtyBindGroupsObjectChangedOrIsDynamic)) {
        DAWN_TRY(ApplyBindGroup(index));
    }
    AfterApply();

    mUsedSlots |= ToBackend(mPipelineLayout)->GetUsedSlots();
    return {};
}

MaybeError BindGroupTracker::ApplyBindGroup(BindGroupIndex index) {
    ID3D11DeviceContext1* deviceContext1 = mCommandContext->GetD3D11DeviceContext1();
    BindGroupBase* group = mBindGroups[index];
    const ityp::vector<BindingIndex, uint64_t>& dynamicOffsets = mDynamicOffsets[index];
    const auto& indices = ToBackend(mPipelineLayout)->GetBindingIndexInfo()[index];

    for (BindingIndex bindingIndex{0}; bindingIndex < group->GetLayout()->GetBindingCount();
         ++bindingIndex) {
        const BindingInfo& bindingInfo = group->GetLayout()->GetBindingInfo(bindingIndex);
        const uint32_t bindingSlot = indices[bindingIndex];

        switch (bindingInfo.bindingType) {
            case BindingInfoType::Buffer: {
                BufferBinding binding = group->GetBindingAsBufferBinding(bindingIndex);
                ID3D11Buffer* d3d11Buffer = ToBackend(binding.buffer)->GetD3D11Buffer();
                auto offset = binding.offset;
                if (bindingInfo.buffer.hasDynamicOffset) {
                    // Dynamic buffers are packed at the front of BindingIndices.
                    offset += dynamicOffsets[bindingIndex];
                }

                switch (bindingInfo.buffer.type) {
                    case wgpu::BufferBindingType::Uniform: {
                        // https://learn.microsoft.com/en-us/windows/win32/api/d3d11_1/nf-d3d11_1-id3d11devicecontext1-vssetconstantbuffers1
                        // Offset and size are measured in shader constants, which are 16 bytes
                        // (4*32-bit components). And the offsets and counts must be multiples
                        // of 16.
                        DAWN_ASSERT(IsAligned(offset, 256));
                        uint32_t firstConstant = static_cast<uint32_t>(offset / 16);
                        uint32_t size = static_cast<uint32_t>(Align(binding.size, 16) / 16);
                        uint32_t numConstants = Align(size, 16);
                        DAWN_ASSERT(offset + numConstants * 16 <=
                                    binding.buffer->GetAllocatedSize());

                        if (bindingInfo.visibility & wgpu::ShaderStage::Vertex) {
                            deviceContext1->VSSetConstantBuffers1(bindingSlot, 1, &d3d11Buffer,
                                                                  &firstConstant, &numConstants);
                        }
                        if (bindingInfo.visibility & wgpu::ShaderStage::Fragment) {
                            deviceContext1->PSSetConstantBuffers1(bindingSlot, 1, &d3d11Buffer,
                                                                  &firstConstant, &numConstants);
                        }
                        if (bindingInfo.visibility & wgpu::ShaderStage::Compute) {
                            deviceContext1->CSSetConstantBuffers1(bindingSlot, 1, &d3d11Buffer,
                                                                  &firstConstant, &numConstants);
                        }
                        break;
                    }
                    case wgpu::BufferBindingType::Storage: {
                        ComPtr<ID3D11UnorderedAccessView> d3d11UAV;
                        DAWN_TRY_ASSIGN(d3d11UAV, ToBackend(binding.buffer)
                                                      ->CreateD3D11UnorderedAccessView1(
                                                          0, binding.buffer->GetSize()));
                        UINT firstElement = offset / 4;
                        if (bindingInfo.visibility & wgpu::ShaderStage::Compute) {
                            deviceContext1->CSSetUnorderedAccessViews(
                                bindingSlot, 1, d3d11UAV.GetAddressOf(), &firstElement);
                        } else {
                            return DAWN_INTERNAL_ERROR(
                                "Storage buffers are only supported in compute shaders");
                        }
                        break;
                    }
                    case wgpu::BufferBindingType::ReadOnlyStorage: {
                        ComPtr<ID3D11ShaderResourceView> d3d11SRV;
                        DAWN_TRY_ASSIGN(d3d11SRV,
                                        ToBackend(binding.buffer)
                                            ->CreateD3D11ShaderResourceView(offset, binding.size));
                        if (bindingInfo.visibility & wgpu::ShaderStage::Vertex) {
                            deviceContext1->VSSetShaderResources(bindingSlot, 1,
                                                                 d3d11SRV.GetAddressOf());
                        }
                        if (bindingInfo.visibility & wgpu::ShaderStage::Fragment) {
                            deviceContext1->PSSetShaderResources(bindingSlot, 1,
                                                                 d3d11SRV.GetAddressOf());
                        }
                        if (bindingInfo.visibility & wgpu::ShaderStage::Compute) {
                            deviceContext1->CSSetShaderResources(bindingSlot, 1,
                                                                 d3d11SRV.GetAddressOf());
                        }
                        break;
                    }
                    case wgpu::BufferBindingType::Undefined:
                        UNREACHABLE();
                }
                break;
            }

            case BindingInfoType::Sampler: {
                Sampler* sampler = ToBackend(group->GetBindingAsSampler(bindingIndex));
                ID3D11SamplerState* d3d11SamplerState = sampler->GetD3D11SamplerState();
                if (bindingInfo.visibility & wgpu::ShaderStage::Vertex) {
                    deviceContext1->VSSetSamplers(bindingSlot, 1, &d3d11SamplerState);
                }
                if (bindingInfo.visibility & wgpu::ShaderStage::Fragment) {
                    deviceContext1->PSSetSamplers(bindingSlot, 1, &d3d11SamplerState);
                }
                if (bindingInfo.visibility & wgpu::ShaderStage::Compute) {
                    deviceContext1->CSSetSamplers(bindingSlot, 1, &d3d11SamplerState);
                }
                break;
            }

            case BindingInfoType::Texture: {
                TextureView* view = ToBackend(group->GetBindingAsTextureView(bindingIndex));
                ComPtr<ID3D11ShaderResourceView> srv;
                DAWN_TRY_ASSIGN(srv, view->CreateD3D11ShaderResourceView());
                if (bindingInfo.visibility & wgpu::ShaderStage::Vertex) {
                    deviceContext1->VSSetShaderResources(bindingSlot, 1, srv.GetAddressOf());
                }
                if (bindingInfo.visibility & wgpu::ShaderStage::Fragment) {
                    deviceContext1->PSSetShaderResources(bindingSlot, 1, srv.GetAddressOf());
                }
                if (bindingInfo.visibility & wgpu::ShaderStage::Compute) {
                    deviceContext1->CSSetShaderResources(bindingSlot, 1, srv.GetAddressOf());
                }
                break;
            }

            case BindingInfoType::StorageTexture: {
                return DAWN_UNIMPLEMENTED_ERROR("Storage textures are not supported");
            }

            case BindingInfoType::ExternalTexture: {
                return DAWN_UNIMPLEMENTED_ERROR("External textures are not supported");
            }
        }
    }
    return {};
}

void BindGroupTracker::GetGroupUsedSlots(BindGroupIndex index,
                                         PipelineLayout::PreStageSlots* usedSlots) {
    BindGroupBase* group = mBindGroups[index];
    const auto& indices = ToBackend(mPipelineLayout)->GetBindingIndexInfo()[index];
    for (BindingIndex bindingIndex{0}; bindingIndex < group->GetLayout()->GetBindingCount();
         ++bindingIndex) {
        const BindingInfo& bindingInfo = group->GetLayout()->GetBindingInfo(bindingIndex);
        const uint32_t bindingSlot = indices[bindingIndex];

        switch (bindingInfo.bindingType) {
            case BindingInfoType::Buffer: {
                switch (bindingInfo.buffer.type) {
                    case wgpu::BufferBindingType::Uniform: {
                        for (SingleShaderStage stage : IterateStages(bindingInfo.visibility)) {
                            (*usedSlots)[stage].mConstantBufferSlots.set(bindingSlot);
                        }
                        break;
                    }
                    case wgpu::BufferBindingType::Storage: {
                        for (SingleShaderStage stage : IterateStages(bindingInfo.visibility)) {
                            (*usedSlots)[stage].mUAVSlots.set(bindingSlot);
                        }
                        break;
                    }
                    case wgpu::BufferBindingType::ReadOnlyStorage: {
                        for (SingleShaderStage stage : IterateStages(bindingInfo.visibility)) {
                            (*usedSlots)[stage].mInputResourceSlots.set(bindingSlot);
                        }
                        break;
                    }
                    case wgpu::BufferBindingType::Undefined:
                        UNREACHABLE();
                }
                break;
            }

            case BindingInfoType::Sampler: {
                for (SingleShaderStage stage : IterateStages(bindingInfo.visibility)) {
                    (*usedSlots)[stage].mSamplerSlots.set(bindingSlot);
                }
                break;
            }

            case BindingInfoType::Texture: {
                for (SingleShaderStage stage : IterateStages(bindingInfo.visibility)) {
                    (*usedSlots)[stage].mInputResourceSlots.set(bindingSlot);
                }
                break;
            }

            case BindingInfoType::StorageTexture:
            case BindingInfoType::ExternalTexture:
                UNREACHABLE();
        }
    }
}

}  // namespace dawn::native::d3d11
