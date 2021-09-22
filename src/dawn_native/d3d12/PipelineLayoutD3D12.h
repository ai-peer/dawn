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

#ifndef DAWNNATIVE_D3D12_PIPELINELAYOUTD3D12_H_
#define DAWNNATIVE_D3D12_PIPELINELAYOUTD3D12_H_

#include "common/Constants.h"
#include "common/ityp_array.h"
#include "common/ityp_span.h"
#include "common/ityp_stack_vec.h"
#include "dawn_native/BindingInfo.h"
#include "dawn_native/PipelineLayout.h"
#include "dawn_native/d3d12/d3d12_platform.h"

#include <map>

namespace dawn_native { namespace d3d12 {

    // We reserve a register space that a user cannot use.
    static constexpr uint32_t kReservedRegisterSpace = kMaxBindGroups + 1;
    static constexpr uint32_t kFirstOffsetInfoBaseRegister = 0;

    class Device;

    class PipelineLayout final : public PipelineLayoutBase {
      public:
        static ResultOrError<Ref<PipelineLayout>> Create(
            Device* device,
            const PipelineLayoutDescriptor* descriptor);

        uint32_t GetCbvUavSrvRootParameterIndex(BindGroupIndex group) const;
        uint32_t GetSamplerRootParameterIndex(BindGroupIndex group) const;

        // Returns the indices of the root parameters reserved for dynamic buffer bindings in
        // |group|.
        ityp::span<BindingIndex, const uint32_t> GetDynamicRootParameterIndices(
            BindGroupIndex group) const;

        uint32_t GetFirstIndexOffsetRegisterSpace() const;
        uint32_t GetFirstIndexOffsetShaderRegister() const;
        uint32_t GetFirstIndexOffsetParameterIndex() const;

        ID3D12RootSignature* GetRootSignature() const;

      private:
        ~PipelineLayout() override = default;
        using PipelineLayoutBase::PipelineLayoutBase;
        MaybeError Initialize();
        ityp::array<BindGroupIndex, uint32_t, kMaxBindGroups> mCbvUavSrvRootParameterInfo;
        ityp::array<BindGroupIndex, uint32_t, kMaxBindGroups> mSamplerRootParameterInfo;
        std::map<BindGroupIndex,
                 ityp::stack_vec<BindingIndex, uint32_t, kMinDynamicBuffersPerPipelineLayout>>
            mDynamicRootParameterIndices;
        uint32_t mFirstIndexOffsetParameterIndex;
        ComPtr<ID3D12RootSignature> mRootSignature;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_PIPELINELAYOUTD3D12_H_
