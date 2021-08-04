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

#include "dawn_native/d3d12/BindGroupLayoutD3D12.h"

#include "common/BitSetIterator.h"
#include "dawn_native/d3d12/BindGroupD3D12.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/SamplerHeapCacheD3D12.h"
#include "dawn_native/d3d12/StagingDescriptorAllocatorD3D12.h"

namespace dawn_native { namespace d3d12 {
    namespace {
        BindGroupLayout::DescriptorType WGPUBindingInfoToDescriptorType(
            const BindingInfo& bindingInfo) {
            switch (bindingInfo.bindingType) {
                case BindingInfoType::Buffer:
                    switch (bindingInfo.buffer.type) {
                        case wgpu::BufferBindingType::Uniform:
                            return BindGroupLayout::DescriptorType::CBV;
                        case wgpu::BufferBindingType::Storage:
                        case kInternalStorageBufferBinding:
                            return BindGroupLayout::DescriptorType::UAV;
                        case wgpu::BufferBindingType::ReadOnlyStorage:
                            return BindGroupLayout::DescriptorType::SRV;
                        case wgpu::BufferBindingType::Undefined:
                            UNREACHABLE();
                    }

                case BindingInfoType::Sampler:
                    return BindGroupLayout::DescriptorType::Sampler;

                case BindingInfoType::Texture:
                case BindingInfoType::ExternalTexture:
                    return BindGroupLayout::DescriptorType::SRV;

                case BindingInfoType::StorageTexture:
                    switch (bindingInfo.storageTexture.access) {
                        case wgpu::StorageTextureAccess::ReadOnly:
                            return BindGroupLayout::DescriptorType::SRV;
                        case wgpu::StorageTextureAccess::WriteOnly:
                            return BindGroupLayout::DescriptorType::UAV;
                        case wgpu::StorageTextureAccess::Undefined:
                            UNREACHABLE();
                    }
            }
        }

        D3D12_DESCRIPTOR_RANGE_TYPE DescriptorTypeToD3D12DescriptorRangeType(
            BindGroupLayout::DescriptorType descriptorType) {
            switch (descriptorType) {
                case BindGroupLayout::DescriptorType::Sampler:
                    return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

                case BindGroupLayout::DescriptorType::CBV:
                    return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

                case BindGroupLayout::DescriptorType::SRV:
                    return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

                case BindGroupLayout::DescriptorType::UAV:
                    return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

                case BindGroupLayout::DescriptorType::Count:
                    DAWN_UNREACHABLE();
            }
        }
    }  // anonymous namespace

    // static
    Ref<BindGroupLayout> BindGroupLayout::Create(Device* device,
                                                 const BindGroupLayoutDescriptor* descriptor) {
        return AcquireRef(new BindGroupLayout(device, descriptor));
    }

    BindGroupLayout::BindGroupLayout(Device* device, const BindGroupLayoutDescriptor* descriptor)
        : BindGroupLayoutBase(device, descriptor),
          mUseBindingAsRegister(false),
          mBindingOffsets(GetBindingCount()),
          mDescriptorCounts{},
          mBindGroupAllocator(MakeFrontendBindGroupAllocator<BindGroup>(4096)) {
        // Samplers are not allowed in the same descriptor heap as non-samplers, so we keep track of
        // their counts separately for descriptor heap offset calculation.
        uint32_t cbvUavSrvCount = 0;
        uint32_t samplerCount = 0;

        for (BindingIndex bindingIndex{0}; bindingIndex < GetBindingCount(); ++bindingIndex) {
            const BindingInfo& bindingInfo = GetBindingInfo(bindingIndex);

            // For dynamic resources, Dawn uses root descriptor in D3D12 backend.
            // So there is no need to allocate the descriptor from descriptor heap.
            if (bindingIndex < GetDynamicBufferCount()) {
                mBindingOffsets[bindingIndex] = uint32_t(bindingInfo.binding);
                continue;
            }

            ASSERT(!bindingInfo.buffer.hasDynamicOffset);

            DescriptorType descriptorType = WGPUBindingInfoToDescriptorType(bindingInfo);

            mBindingOffsets[bindingIndex] =
                descriptorType == DescriptorType::Sampler ? samplerCount++ : cbvUavSrvCount++;

            D3D12_DESCRIPTOR_RANGE range;
            range.RangeType = DescriptorTypeToD3D12DescriptorRangeType(descriptorType);
            range.NumDescriptors = 1;
            range.BaseShaderRegister = mUseBindingAsRegister ? uint32_t(bindingInfo.binding)
                                                             : mBindingOffsets[bindingIndex];
            range.RegisterSpace = REGISTER_SPACE_PLACEHOLDER;
            range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            std::vector<D3D12_DESCRIPTOR_RANGE>& descriptorRanges =
                descriptorType == DescriptorType::Sampler ? mSamplerDescriptorRanges
                                                          : mCbvUavSrvDescriptorRanges;
            descriptorRanges.push_back(range);

            mDescriptorCounts[descriptorType]++;
        }

        // Attempt to reduce the size of `ranges` by merging contiguous decriptor ranges.
        auto MergeDescriptorRanges = [](std::vector<D3D12_DESCRIPTOR_RANGE>& ranges) {
            if (ranges.size() < 2) {
                return;
            }

            auto it = ranges.begin();
            ++it;  // Skip first element so we can have a window of size 2
            while (it != ranges.end()) {
                auto current = it;
                auto previous = it - 1;

                // Try to join this range with the previous one, if the types are equal and the
                // current range is a continuation of the previous.
                if (previous->RangeType == current->RangeType &&
                    current->BaseShaderRegister - previous->NumDescriptors ==
                        previous->BaseShaderRegister) {
                    ASSERT(current->OffsetInDescriptorsFromTableStart ==
                               D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND ||
                           previous->OffsetInDescriptorsFromTableStart + previous->NumDescriptors ==
                               current->OffsetInDescriptorsFromTableStart);
                    previous->NumDescriptors += current->NumDescriptors;
                    it = ranges.erase(it);
                } else {
                    ++it;
                }
            }
        };

        MergeDescriptorRanges(mCbvUavSrvDescriptorRanges);
        MergeDescriptorRanges(mSamplerDescriptorRanges);

        mViewAllocator = device->GetViewStagingDescriptorAllocator(GetCbvUavSrvDescriptorCount());
        mSamplerAllocator =
            device->GetSamplerStagingDescriptorAllocator(GetSamplerDescriptorCount());
    }

    ResultOrError<Ref<BindGroup>> BindGroupLayout::AllocateBindGroup(
        Device* device,
        const BindGroupDescriptor* descriptor) {
        uint32_t viewSizeIncrement = 0;
        CPUDescriptorHeapAllocation viewAllocation;
        if (GetCbvUavSrvDescriptorCount() > 0) {
            DAWN_TRY_ASSIGN(viewAllocation, mViewAllocator->AllocateCPUDescriptors());
            viewSizeIncrement = mViewAllocator->GetSizeIncrement();
        }

        Ref<BindGroup> bindGroup = AcquireRef<BindGroup>(
            mBindGroupAllocator.Allocate(device, descriptor, viewSizeIncrement, viewAllocation));

        if (GetSamplerDescriptorCount() > 0) {
            Ref<SamplerHeapCacheEntry> samplerHeapCacheEntry;
            DAWN_TRY_ASSIGN(samplerHeapCacheEntry, device->GetSamplerHeapCache()->GetOrCreate(
                                                       bindGroup.Get(), mSamplerAllocator));
            bindGroup->SetSamplerAllocationEntry(std::move(samplerHeapCacheEntry));
        }

        return bindGroup;
    }

    void BindGroupLayout::DeallocateBindGroup(BindGroup* bindGroup,
                                              CPUDescriptorHeapAllocation* viewAllocation) {
        if (viewAllocation->IsValid()) {
            mViewAllocator->Deallocate(viewAllocation);
        }

        mBindGroupAllocator.Deallocate(bindGroup);
    }

    ityp::span<BindingIndex, const uint32_t> BindGroupLayout::GetDescriptorHeapOffsets() const {
        return {mBindingOffsets.data(), mBindingOffsets.size()};
    }

    uint32_t BindGroupLayout::GetShaderRegister(BindingIndex bindingIndex) const {
        return mUseBindingAsRegister ? static_cast<uint32_t>(GetBindingInfo(bindingIndex).binding)
                                     : mBindingOffsets[bindingIndex];
    }

    ityp::span<BindingIndex, const uint32_t> BindGroupLayout::GetDynamicBindingRegisters() const {
        // This is safe because the dynamic buffers are packed to the front of the bindings and
        // `GetDynamicBufferCount()` will never be greater than the number of bindings
        return {mBindingOffsets.data(), GetDynamicBufferCount()};
    }

    uint32_t BindGroupLayout::GetCbvUavSrvDescriptorCount() const {
        return mDescriptorCounts[CBV] + mDescriptorCounts[UAV] + mDescriptorCounts[SRV];
    }

    uint32_t BindGroupLayout::GetSamplerDescriptorCount() const {
        return mDescriptorCounts[Sampler];
    }

    ityp::span<size_t, const D3D12_DESCRIPTOR_RANGE> BindGroupLayout::GetCbvUavSrvDescriptorRanges()
        const {
        return {mCbvUavSrvDescriptorRanges.data(), mCbvUavSrvDescriptorRanges.size()};
    }

    ityp::span<size_t, const D3D12_DESCRIPTOR_RANGE> BindGroupLayout::GetSamplerDescriptorRanges()
        const {
        return {mSamplerDescriptorRanges.data(), mSamplerDescriptorRanges.size()};
    }

}}  // namespace dawn_native::d3d12
