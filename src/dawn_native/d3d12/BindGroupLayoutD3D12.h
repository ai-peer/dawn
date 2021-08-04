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

    // A purposefully invalid register space. The real space is set in the pipeline layout based on
    // the bind group index.
    const uint32_t REGISTER_SPACE_PLACEHOLDER = D3D12_DRIVER_RESERVED_REGISTER_SPACE_VALUES_START;

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

        // The offset (in descriptor count) into the corresponding descriptor heap. Not valid for
        // dynamic binding indexes.
        ityp::span<BindingIndex, const uint32_t> GetDescriptorHeapOffsets() const;

        // The D3D shader register that the Dawn binding index is mapped to by this bind group
        // layout.
        uint32_t GetShaderRegister(BindingIndex bindingIndex) const;

        uint32_t GetCbvUavSrvDescriptorCount() const;
        uint32_t GetSamplerDescriptorCount() const;
        ityp::span<size_t, const D3D12_DESCRIPTOR_RANGE> GetCbvUavSrvDescriptorRanges() const;
        ityp::span<size_t, const D3D12_DESCRIPTOR_RANGE> GetSamplerDescriptorRanges() const;

      private:
        BindGroupLayout(Device* device, const BindGroupLayoutDescriptor* descriptor);
        ~BindGroupLayout() override = default;

        // If `true`, use the WGSL binding numbers directly as the HLSL/DXIL shader registers.
        // If `false`, compact the register space so there are no holes in either the CBV/UAV/SRV
        // group or the Sampler group.
        //
        // When targetting shader model <=5.0, the max valid register index ("slot count") is
        // relatively low for each resource type, so compacting the space is beneficial in that
        // case.
        bool mUseBindingAsRegister;

        // - For non-dynamic resources, contains the offset into the descriptor heap for the given
        // resource view.
        // - For dynamic resources, contains the shader register.
        //
        // In the `mUseBindingAsRegister = false` case, this is also equal to the remapped shader
        // register.
        ityp::stack_vec<BindingIndex, uint32_t, kMaxOptimalBindingsPerGroup> mBindingOffsets;
        std::array<uint32_t, DescriptorType::Count> mDescriptorCounts;

        std::vector<D3D12_DESCRIPTOR_RANGE> mCbvUavSrvDescriptorRanges;
        std::vector<D3D12_DESCRIPTOR_RANGE> mSamplerDescriptorRanges;

        SlabAllocator<BindGroup> mBindGroupAllocator;

        StagingDescriptorAllocator* mSamplerAllocator = nullptr;
        StagingDescriptorAllocator* mViewAllocator = nullptr;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_BINDGROUPLAYOUTD3D12_H_
