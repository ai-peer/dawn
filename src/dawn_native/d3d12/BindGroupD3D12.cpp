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
#include "dawn_native/d3d12/SamplerD3D12.h"
#include "dawn_native/d3d12/ShaderVisibleDescriptorAllocator.h"
#include "dawn_native/d3d12/TextureD3D12.h"

#include "dawn_native/d3d12/DeviceD3D12.h"

namespace dawn_native { namespace d3d12 {

    BindGroup::BindGroup(Device* device, const BindGroupDescriptor* descriptor)
        : BindGroupBase(device, descriptor) {
    }

    bool BindGroup::TryAllocateIfNeeded(ShaderVisibleDescriptorAllocator* allocator) {

        // Reuse the existing allocation if it is still valid.
        ShaderVisibleDescriptorAllocator::HeapSerials serials =
            allocator->GetCurrentHeapSerials();
        if (mCbvSrvUavHeapSerial == serials.cbvUavSrvSerial &&
            mSamplerHeapSerial == serials.samplerSerial) {
            return true;
        }

        const BindGroupLayout* bgl = ToBackend(GetLayout());
        const uint32_t samplerDescriptorCount = bgl->GetSamplerDescriptorCount();
        const uint32_t cbvUavSrvDescriptorCount = bgl->GetCbvUavSrvDescriptorCount();

        // The allocation isn't valid, ask for a new one.
        ShaderVisibleDescriptorAllocation cbvUavSrvAllocation =
            allocator->AllocateCbvUavSrcDescriptors(cbvUavSrvDescriptorCount);
        ShaderVisibleDescriptorAllocation samplerAllocation =
            allocator->AllocateSamplerDescriptors(samplerDescriptorCount);

        // If either allocations failed, bail out.
        if (!cbvUavSrvAllocation.IsValid() || !samplerAllocation.IsValid()) {
            return false;
        }

        // Populate the allocations.
        const auto& layout = bgl->GetBindingInfo();
        const auto& bindingOffsets = bgl->GetBindingOffsets();
        ID3D12Device* d3d12Device = ToBackend(GetDevice())->GetD3D12Device().Get();

        for (uint32_t bindingIndex : IterateBitSet(layout.mask)) {
            // It's not necessary to create descriptors in descriptor heap for dynamic resources.
            // So skip allocating descriptors in descriptor heaps for dynamic buffers.
            if (layout.hasDynamicOffset[bindingIndex]) {
                continue;
            }

            switch (layout.types[bindingIndex]) {
                case wgpu::BindingType::UniformBuffer: {
                    BufferBinding binding = GetBindingAsBufferBinding(bindingIndex);

                    D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
                    // TODO(enga@google.com): investigate if this needs to be a constraint at the
                    // API level
                    desc.SizeInBytes = Align(binding.size, 256);
                    desc.BufferLocation = ToBackend(binding.buffer)->GetVA() + binding.offset;

                    d3d12Device->CreateConstantBufferView(
                        &desc, cbvUavSrvAllocation.GetCPUHandle(bindingOffsets[bindingIndex]));
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
                        cbvUavSrvAllocation.GetCPUHandle(bindingOffsets[bindingIndex]));
                } break;
                case wgpu::BindingType::ReadonlyStorageBuffer: {
                    BufferBinding binding = GetBindingAsBufferBinding(bindingIndex);

                    // Like StorageBuffer, SPIRV-Cross outputs HLSL shaders for readonly storage
                    // buffer with ByteAddressBuffer. So we must use D3D12_BUFFER_SRV_FLAG_RAW
                    // when making the SRV descriptor. And it has similar requirement for format,
                    // element size, etc.
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
                        cbvUavSrvAllocation.GetCPUHandle(bindingOffsets[bindingIndex]));
                } break;
                case wgpu::BindingType::SampledTexture: {
                    auto* view = ToBackend(GetBindingAsTextureView(bindingIndex));
                    auto& srv = view->GetSRVDescriptor();
                    d3d12Device->CreateShaderResourceView(
                        ToBackend(view->GetTexture())->GetD3D12Resource(), &srv,
                        cbvUavSrvAllocation.GetCPUHandle(bindingOffsets[bindingIndex]));
                } break;
                case wgpu::BindingType::Sampler: {
                    auto* sampler = ToBackend(GetBindingAsSampler(bindingIndex));
                    auto& samplerDesc = sampler->GetSamplerDescriptor();
                    d3d12Device->CreateSampler(
                        &samplerDesc, samplerAllocation.GetCPUHandle(bindingOffsets[bindingIndex]));
                } break;

                case wgpu::BindingType::StorageTexture:
                    UNREACHABLE();
                    break;
            }
        }

        // Save the handle to the start of the descriptor table in the heap
        // Upon ApplyBindGroup(), these handles are re-used should the bindgroup remain allocated on
        // the same heap.
        mCbvUavSrvBaseDescriptor = cbvUavSrvAllocation.GetGPUHandle(0);
        mCbvSrvUavHeapSerial = serials.cbvUavSrvSerial;
        mSamplerBaseDescriptor = samplerAllocation.GetGPUHandle(0);
        mSamplerHeapSerial = serials.samplerSerial;

        return true;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE BindGroup::GetCbvUavSrvBaseDescriptor() const {
        return mCbvUavSrvBaseDescriptor;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE BindGroup::GetSamplerHeapBaseDescriptor() const {
        return mSamplerBaseDescriptor;
    }

}}  // namespace dawn_native::d3d12
