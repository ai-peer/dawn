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

#include "dawn_native/d3d12/BindGroupD3D12.h"
#include "common/BitSetIterator.h"
#include "dawn_native/d3d12/BindGroupLayoutD3D12.h"
#include "dawn_native/d3d12/BufferD3D12.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/SamplerD3D12.h"
#include "dawn_native/d3d12/TextureD3D12.h"

namespace dawn_native { namespace d3d12 {

    BindGroup::BindGroup(Device* device, const BindGroupDescriptor* descriptor)
        : BindGroupBase(device, descriptor) {
    }

    // Returns true if the BindGroup was successfully created.
    ResultOrError<bool> BindGroup::Create() {
        Device* device = ToBackend(GetDevice());

        // Bindgroup does not need to be re-created should it's allocation remain valid.
        if (device->IsBindGroupInvalidated(mCbvSrvUavHeapAllocation) &&
            device->IsBindGroupInvalidated(mSamplerHeapAllocation)) {
            return true;
        }

        const auto* bgl = ToBackend(GetLayout());

        DescriptorHeapAllocation cbvSrvUavDescriptorHeapAllocation;
        DAWN_TRY_ASSIGN(cbvSrvUavDescriptorHeapAllocation,
                        device->AllocateMemory(bgl->GetCbvUavSrvDescriptorCount(),
                                               D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

        DescriptorHeapAllocation samplerDescriptorHeapAllocation;
        DAWN_TRY_ASSIGN(samplerDescriptorHeapAllocation,
                        device->AllocateMemory(bgl->GetSamplerDescriptorCount(),
                                               D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER));

        // If either allocation failed, allocator must re-create the heap since
        // all created bindgroups must be created on the same heap.
        if (cbvSrvUavDescriptorHeapAllocation.Get() == nullptr ||
            samplerDescriptorHeapAllocation.Get() == nullptr) {
            return false;
        }

        mCbvSrvUavHeapAllocation = cbvSrvUavDescriptorHeapAllocation;
        mSamplerHeapAllocation = samplerDescriptorHeapAllocation;

        const auto& layout = bgl->GetBindingInfo();

        const auto& bindingOffsets = bgl->GetBindingOffsets();

        ID3D12Device* d3d12Device = device->GetD3D12Device().Get();

        for (uint32_t bindingIndex : IterateBitSet(layout.mask)) {
            // It's not necessary to create descriptors in descriptor heap for dynamic
            // resources. So skip allocating descriptors in descriptor heaps for dynamic
            // buffers.
            if (layout.hasDynamicOffset[bindingIndex]) {
                continue;
            }

            switch (layout.types[bindingIndex]) {
                case wgpu::BindingType::UniformBuffer: {
                    BufferBinding binding = GetBindingAsBufferBinding(bindingIndex);

                    D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
                    // TODO(enga@google.com): investigate if this needs to be a constraint at
                    // the API level
                    desc.SizeInBytes = Align(binding.size, 256);
                    desc.BufferLocation = ToBackend(binding.buffer)->GetVA() + binding.offset;

                    d3d12Device->CreateConstantBufferView(
                        &desc, cbvSrvUavDescriptorHeapAllocation.GetCPUHandle(
                                   bindingOffsets[bindingIndex]));
                } break;
                case wgpu::BindingType::StorageBuffer: {
                    BufferBinding binding = GetBindingAsBufferBinding(bindingIndex);

                    // Since SPIRV-Cross outputs HLSL shaders with RWByteAddressBuffer,
                    // we must use D3D12_BUFFER_UAV_FLAG_RAW when making the
                    // UNORDERED_ACCESS_VIEW_DESC. Using D3D12_BUFFER_UAV_FLAG_RAW requires
                    // that we use DXGI_FORMAT_R32_TYPELESS as the format of the view.
                    // DXGI_FORMAT_R32_TYPELESS requires that the element size be 4
                    // byte aligned. Since binding.size and binding.offset are in bytes,
                    // we need to divide by 4 to obtain the element size.
                    D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
                    desc.Buffer.NumElements = binding.size / 4;
                    desc.Format = DXGI_FORMAT_R32_TYPELESS;
                    desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                    desc.Buffer.FirstElement = binding.offset / 4;
                    desc.Buffer.StructureByteStride = 0;
                    desc.Buffer.CounterOffsetInBytes = 0;
                    desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

                    d3d12Device->CreateUnorderedAccessView(
                        ToBackend(binding.buffer)->GetD3D12Resource().Get(), nullptr, &desc,
                        cbvSrvUavDescriptorHeapAllocation.GetCPUHandle(
                            bindingOffsets[bindingIndex]));
                } break;
                case wgpu::BindingType::ReadonlyStorageBuffer: {
                    BufferBinding binding = GetBindingAsBufferBinding(bindingIndex);

                    // Like StorageBuffer, SPIRV-Cross outputs HLSL shaders for readonly storage
                    // buffer with ByteAddressBuffer. So we must use D3D12_BUFFER_SRV_FLAG_RAW
                    // when making the SRV descriptor. And it has similar requirement for
                    // format, element size, etc.
                    D3D12_SHADER_RESOURCE_VIEW_DESC desc;
                    desc.Format = DXGI_FORMAT_R32_TYPELESS;
                    desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    desc.Buffer.FirstElement = binding.offset / 4;
                    desc.Buffer.NumElements = binding.size / 4;
                    desc.Buffer.StructureByteStride = 0;
                    desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
                    d3d12Device->CreateShaderResourceView(
                        ToBackend(binding.buffer)->GetD3D12Resource().Get(), &desc,
                        cbvSrvUavDescriptorHeapAllocation.GetCPUHandle(
                            bindingOffsets[bindingIndex]));
                } break;
                case wgpu::BindingType::SampledTexture: {
                    auto* view = ToBackend(GetBindingAsTextureView(bindingIndex));
                    auto& srv = view->GetSRVDescriptor();
                    d3d12Device->CreateShaderResourceView(
                        ToBackend(view->GetTexture())->GetD3D12Resource(), &srv,
                        cbvSrvUavDescriptorHeapAllocation.GetCPUHandle(
                            bindingOffsets[bindingIndex]));
                } break;
                case wgpu::BindingType::Sampler: {
                    auto* sampler = ToBackend(GetBindingAsSampler(bindingIndex));
                    auto& samplerDesc = sampler->GetSamplerDescriptor();
                    d3d12Device->CreateSampler(
                        &samplerDesc,
                        samplerDescriptorHeapAllocation.GetCPUHandle(bindingOffsets[bindingIndex]));
                } break;

                case wgpu::BindingType::StorageTexture:
                    UNREACHABLE();
                    break;

                    // TODO(shaobo.yan@intel.com): Implement dynamic buffer offset.
            }
        }

        return true;
    }

    DescriptorHeapAllocation BindGroup::GetCbvUavSrvHeapAllocation() const {
        ASSERT(mCbvSrvUavHeapAllocation.GetType() == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        return mCbvSrvUavHeapAllocation;
    }

    DescriptorHeapAllocation BindGroup::GetSamplerHeapAllocation() const {
        ASSERT(mSamplerHeapAllocation.GetType() == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        return mSamplerHeapAllocation;
    }
}}  // namespace dawn_native::d3d12
