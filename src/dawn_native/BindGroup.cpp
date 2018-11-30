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

#include "dawn_native/BindGroup.h"

#include "common/Assert.h"
#include "common/Math.h"
#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/Device.h"
#include "dawn_native/Sampler.h"
#include "dawn_native/Texture.h"

namespace dawn_native {

    namespace {

        MaybeError ValidateBufferBinding(const BindGroupBinding& binding, dawn::BufferUsageBit requiredUsage) {
            if (binding.bufferView == nullptr) {
                return DAWN_VALIDATION_ERROR("expected buffer binding");
            }

            if (!IsAligned(binding.bufferView->GetOffset(), 256)) {
                return DAWN_VALIDATION_ERROR("Buffer view offset for bind group needs to be 256-byte aligned");
            }

            if (!(binding.bufferView->GetBuffer()->GetUsage() & requiredUsage)) {
                return DAWN_VALIDATION_ERROR("buffer binding usage mismatch");
            }

            return {};
        }

        MaybeError ValidateTextureBinding(const BindGroupBinding& binding, dawn::TextureUsageBit requiredUsage) {
            if (binding.textureView == nullptr) {
                return DAWN_VALIDATION_ERROR("expected texture binding");
            }

            if (!(binding.textureView->GetTexture()->GetUsage() & requiredUsage)) {
                return DAWN_VALIDATION_ERROR("texture binding usage mismatch");
            }

            return {};
        }

    }  // anonymous namespace

    MaybeError ValidateBindGroupDescriptor(DeviceBase*, const BindGroupDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }

        if (descriptor->numBindings > kMaxBindingsPerGroup) {
            return DAWN_VALIDATION_ERROR("too many bindings");
        }

        BindGroupLayoutBase::LayoutBindingInfo layoutInfo = descriptor->layout->GetBindingInfo();

        std::bitset<kMaxBindingsPerGroup> bindingsSet;
        for (uint32_t i = 0; i < descriptor->numBindings; ++i) {
            const BindGroupBinding& binding = descriptor->bindings[i];
            uint32_t bindingIndex = binding.binding;

            if (bindingIndex >= kMaxBindingsPerGroup) {
                return DAWN_VALIDATION_ERROR("binding index too high");
            }

            if (!layoutInfo.mask[bindingIndex]) {
                return DAWN_VALIDATION_ERROR("setting non-existent binding");
            }

            if (bindingsSet[bindingIndex]) {
                return DAWN_VALIDATION_ERROR("binding set twice");
            }
            bindingsSet.set(bindingIndex);

            size_t numBindingTypes = 0;
            numBindingTypes += (binding.bufferView == nullptr) ? 0 : 1;
            numBindingTypes += (binding.textureView == nullptr) ? 0 : 1;
            numBindingTypes += (binding.sampler == nullptr) ? 0 : 1;

            if (numBindingTypes != 1) {
                return DAWN_VALIDATION_ERROR("expected only one binding to be set");
            }

            switch (layoutInfo.types[bindingIndex]) {
                case dawn::BindingType::UniformBuffer:
                    DAWN_TRY(ValidateBufferBinding(binding, dawn::BufferUsageBit::Uniform));
                    break;
                case dawn::BindingType::StorageBuffer:
                    DAWN_TRY(ValidateBufferBinding(binding, dawn::BufferUsageBit::Storage));
                    break;
                case dawn::BindingType::SampledTexture:
                    DAWN_TRY(ValidateTextureBinding(binding, dawn::TextureUsageBit::Sampled));
                    break;
                case dawn::BindingType::Sampler:
                    if (binding.sampler == nullptr) {
                        return DAWN_VALIDATION_ERROR("expected sampler binding");
                    }
                    break;
            }
        }

        if (bindingsSet != layoutInfo.mask) {
            return DAWN_VALIDATION_ERROR("bindings missing");
        }

        return {};
    }

    // BindGroup

    BindGroupBase::BindGroupBase(DeviceBase* device, const BindGroupDescriptor* descriptor)
        : ObjectBase(device),
          mLayout(descriptor->layout) {
        for (uint32_t i = 0; i < descriptor->numBindings; ++i) {
            const BindGroupBinding& binding = descriptor->bindings[i];

            uint32_t bindingIndex = binding.binding;
            ASSERT(bindingIndex < kMaxBindingsPerGroup);

            if (binding.bufferView != nullptr) {
                ASSERT(mBindings[bindingIndex].Get() == nullptr);
                mBindings[bindingIndex] = binding.bufferView;
                break;
            }

            if (binding.textureView != nullptr) {
                ASSERT(mBindings[bindingIndex].Get() == nullptr);
                mBindings[bindingIndex] = binding.textureView;
                break;
            }

            if (binding.sampler != nullptr) {
                ASSERT(mBindings[bindingIndex].Get() == nullptr);
                mBindings[bindingIndex] = binding.sampler;
                break;
            }
        }
    }

    const BindGroupLayoutBase* BindGroupBase::GetLayout() const {
        return mLayout.Get();
    }

    BufferViewBase* BindGroupBase::GetBindingAsBufferView(size_t binding) {
        ASSERT(binding < kMaxBindingsPerGroup);
        ASSERT(mLayout->GetBindingInfo().mask[binding]);
        ASSERT(mLayout->GetBindingInfo().types[binding] == dawn::BindingType::UniformBuffer ||
               mLayout->GetBindingInfo().types[binding] == dawn::BindingType::StorageBuffer);
        return reinterpret_cast<BufferViewBase*>(mBindings[binding].Get());
    }

    SamplerBase* BindGroupBase::GetBindingAsSampler(size_t binding) {
        ASSERT(binding < kMaxBindingsPerGroup);
        ASSERT(mLayout->GetBindingInfo().mask[binding]);
        ASSERT(mLayout->GetBindingInfo().types[binding] == dawn::BindingType::Sampler);
        return reinterpret_cast<SamplerBase*>(mBindings[binding].Get());
    }

    TextureViewBase* BindGroupBase::GetBindingAsTextureView(size_t binding) {
        ASSERT(binding < kMaxBindingsPerGroup);
        ASSERT(mLayout->GetBindingInfo().mask[binding]);
        ASSERT(mLayout->GetBindingInfo().types[binding] == dawn::BindingType::SampledTexture);
        return reinterpret_cast<TextureViewBase*>(mBindings[binding].Get());
    }
}  // namespace dawn_native
