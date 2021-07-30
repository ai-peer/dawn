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

#ifndef DAWNNATIVE_D3D12_BINDGROUPLAYOUTD3D12_H_
#define DAWNNATIVE_D3D12_BINDGROUPLAYOUTD3D12_H_

#include "dawn_native/BindGroupLayout.h"

#include "common/SlabAllocator.h"
#include "common/ityp_stack_vec.h"
#include "dawn_native/d3d12/d3d12_platform.h"

namespace dawn_native { namespace d3d12 {

    class BindGroup;
    class CPUDescriptorHeapAllocation;
    class Device;
    class StagingDescriptorAllocator;

    class BindGroupLayout final : public BindGroupLayoutBase {
      public:
        static Ref<BindGroupLayout> Create(Device* device,
                                           const BindGroupLayoutDescriptor* descriptor);

        ResultOrError<Ref<BindGroup>> AllocateBindGroup(Device* device,
                                                        const BindGroupDescriptor* descriptor);
        void DeallocateBindGroup(BindGroup* bindGroup, CPUDescriptorHeapAllocation* viewAllocation);

        enum DescriptorType {
            CBV,
            UAV,
            SRV,
            Sampler,
            Count,
        };

        // This maps binding indices for:
        // - non-dynamic resources, to the offset into the CBV/UAV/SRV or sampler descriptor heap.
        // - dynamic resources, to the shader registor the the resource is bound to.
        ityp::span<BindingIndex, const uint32_t> GetBindingOffsets() const;
        uint32_t GetCbvUavSrvDescriptorCount() const;
        uint32_t GetSamplerDescriptorCount() const;
        ityp::span<size_t, const D3D12_DESCRIPTOR_RANGE> GetCbvUavSrvDescriptorRanges() const;
        ityp::span<size_t, const D3D12_DESCRIPTOR_RANGE> GetSamplerDescriptorRanges() const;

        // Returns a single register in this space that is not used.
        uint32_t GetAvailableRegister() const;

      private:
        BindGroupLayout(Device* device, const BindGroupLayoutDescriptor* descriptor);
        ~BindGroupLayout() override = default;
        ityp::stack_vec<BindingIndex, uint32_t, kMaxOptimalBindingsPerGroup> mBindingOffsets;
        std::array<uint32_t, DescriptorType::Count> mDescriptorCounts;

        std::vector<D3D12_DESCRIPTOR_RANGE> mCbvUavSrvDescriptorRanges;
        std::vector<D3D12_DESCRIPTOR_RANGE> mSamplerDescriptorRanges;

        // Note: we use "next available" instead of "max", so this remains valid when the bind group
        // layout is empty.
        uint32_t mNextAvailableRegister = 0;

        SlabAllocator<BindGroup> mBindGroupAllocator;

        StagingDescriptorAllocator* mSamplerAllocator = nullptr;
        StagingDescriptorAllocator* mViewAllocator = nullptr;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_BINDGROUPLAYOUTD3D12_H_
