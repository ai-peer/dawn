// Copyright 2019 The Dawn Authors
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

#ifndef DAWNNATIVE_BINDGROUPANDUSAGETRACKER_H_
#define DAWNNATIVE_BINDGROUPANDUSAGETRACKER_H_

#include "dawn_native/BindGroupTracker.h"

#include "dawn_native/BindGroup.h"

namespace dawn_native {

    // Extends BindGroupTrackerBase to also keep track of resources that need a usage transition.
    template <bool CanInheritBindGroups, typename DynamicOffset = uint64_t>
    class BindGroupAndUsageTrackerBase
        : public BindGroupTrackerBase<CanInheritBindGroups, DynamicOffset> {
        using Base = BindGroupTrackerBase<CanInheritBindGroups, DynamicOffset>;

      public:
        void OnSetBindGroup(uint32_t index,
                            BindGroupBase* bindGroup,
                            uint32_t dynamicOffsetCount,
                            uint64_t* dynamicOffsets) {
            if (this->mBindGroups[index] != bindGroup) {
                mBuffers[index] = {};
                mBuffersNeedingTransition[index] = {};

                const BindGroupLayoutBase* layout = bindGroup->GetLayout();
                const auto& info = layout->GetBindingInfo();

                for (uint32_t binding : IterateBitSet(info.mask)) {
                    if ((info.visibilities[binding] & dawn::ShaderStage::Compute) == 0) {
                        continue;
                    }

                    mBindingTypes[index][binding] = info.types[binding];
                    switch (info.types[binding]) {
                        case dawn::BindingType::UniformBuffer:
                        case dawn::BindingType::StorageBuffer:
                            mBuffersNeedingTransition[index].set(binding);
                            mBuffers[index][binding] =
                                bindGroup->GetBindingAsBufferBinding(binding).buffer;
                            break;

                        case dawn::BindingType::ReadonlyStorageBuffer:
                        case dawn::BindingType::Sampler:
                        case dawn::BindingType::SampledTexture:
                        case dawn::BindingType::StorageTexture:
                            // Not implemented.
                        default:
                            UNREACHABLE();
                            break;
                    }
                }
            }

            Base::OnSetBindGroup(index, bindGroup, dynamicOffsetCount, dynamicOffsets);
        }

      protected:
        void DidApply() {
            for (uint32_t bgl : IterateBitSet(Base::mBindGroupLayoutsMask)) {
                for (uint32_t binding : IterateBitSet(mBuffersNeedingTransition[bgl])) {
                    switch (mBindingTypes[bgl][binding]) {
                        case dawn::BindingType::UniformBuffer:
                            // The buffer is readonly. Reset the flag because it won't need a
                            // transition again.
                            mBuffersNeedingTransition[bgl].reset(binding);
                            break;

                        case dawn::BindingType::StorageBuffer:
                            // The buffer is writable. Don't reset the flag because we need to
                            // transition for subsequent dispatches.
                            break;

                        case dawn::BindingType::ReadonlyStorageBuffer:
                        case dawn::BindingType::Sampler:
                        case dawn::BindingType::SampledTexture:
                        case dawn::BindingType::StorageTexture:
                            // Not implemented.
                        default:
                            UNREACHABLE();
                            break;
                    }
                }
            }
            Base::DidApply();
        }

        std::array<std::bitset<kMaxBindingsPerGroup>, kMaxBindGroups> mBuffersNeedingTransition =
            {};
        std::array<std::array<dawn::BindingType, kMaxBindingsPerGroup>, kMaxBindGroups>
            mBindingTypes = {};
        std::array<std::array<BufferBase*, kMaxBindingsPerGroup>, kMaxBindGroups> mBuffers = {};
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_BINDGROUPANDUSAGETRACKER_H_
