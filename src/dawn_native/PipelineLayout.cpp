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

#include "dawn_native/PipelineLayout.h"

#include "common/Assert.h"
#include "common/BitSetIterator.h"
#include "common/HashUtils.h"
#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/Device.h"
#include "dawn_native/ShaderModule.h"

namespace dawn_native {

    MaybeError ValidatePipelineLayoutDescriptor(DeviceBase* device,
                                                const PipelineLayoutDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }

        if (descriptor->bindGroupLayoutCount > kMaxBindGroups) {
            return DAWN_VALIDATION_ERROR("too many bind group layouts");
        }

        uint32_t totalDynamicUniformBufferCount = 0;
        uint32_t totalDynamicStorageBufferCount = 0;
        for (uint32_t i = 0; i < descriptor->bindGroupLayoutCount; ++i) {
            if (descriptor->bindGroupLayouts[i] == nullptr) {
                continue;
            }
            DAWN_TRY(device->ValidateObject(descriptor->bindGroupLayouts[i]));
            totalDynamicUniformBufferCount +=
                descriptor->bindGroupLayouts[i]->GetDynamicUniformBufferCount();
            totalDynamicStorageBufferCount +=
                descriptor->bindGroupLayouts[i]->GetDynamicStorageBufferCount();
        }

        if (totalDynamicUniformBufferCount > kMaxDynamicUniformBufferCount) {
            return DAWN_VALIDATION_ERROR("too many dynamic uniform buffers in pipeline layout");
        }

        if (totalDynamicStorageBufferCount > kMaxDynamicStorageBufferCount) {
            return DAWN_VALIDATION_ERROR("too many dynamic storage buffers in pipeline layout");
        }

        return {};
    }

    // PipelineLayoutBase

    PipelineLayoutBase::PipelineLayoutBase(DeviceBase* device,
                                           const PipelineLayoutDescriptor* descriptor)
        : CachedObject(device) {
        ASSERT(descriptor->bindGroupLayoutCount <= kMaxBindGroups);
        for (uint32_t group = 0; group < descriptor->bindGroupLayoutCount; ++group) {
            if (descriptor->bindGroupLayouts[group] == nullptr) {
                continue;
            }
            mBindGroupLayouts[group] = descriptor->bindGroupLayouts[group];
            mMask.set(group);
        }
    }

    PipelineLayoutBase::PipelineLayoutBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : CachedObject(device, tag) {
    }

    PipelineLayoutBase::~PipelineLayoutBase() {
        // Do not uncache the actual cached object if we are a blueprint
        if (IsCachedReference()) {
            GetDevice()->UncachePipelineLayout(this);
        }
    }

    // static
    PipelineLayoutBase* PipelineLayoutBase::MakeError(DeviceBase* device) {
        return new PipelineLayoutBase(device, ObjectBase::kError);
    }

    // static
    ResultOrError<PipelineLayoutBase*> PipelineLayoutBase::CreateDefault(
        DeviceBase* device,
        const ShaderModuleBase* const* modules,
        uint32_t count) {
        ASSERT(count > 0);

        std::array<std::array<BindGroupLayoutBinding, kMaxBindingsPerGroup>, kMaxBindGroups>
            bindings = {};
        std::array<std::bitset<kMaxBindingsPerGroup>, kMaxBindGroups> usedBindings = {};
        std::array<uint32_t, kMaxBindGroups> bindingCounts = {};

        uint32_t bindGroupLayoutCount = 0;
        for (uint32_t moduleIndex = 0; moduleIndex < count; ++moduleIndex) {
            const ShaderModuleBase* module = modules[moduleIndex];
            const ShaderModuleBase::ModuleBindingInfo& info = module->GetBindingInfo();

            for (uint32_t group = 0; group < info.size(); ++group) {
                for (uint32_t binding = 0; binding < info[group].size(); ++binding) {
                    const ShaderModuleBase::BindingInfo& bindingInfo = info[group][binding];
                    if (!bindingInfo.used) {
                        continue;
                    }

                    if (bindingInfo.multisampled) {
                        return DAWN_VALIDATION_ERROR("Multisampled textures not supported (yet)");
                    }

                    if (usedBindings[group][binding]) {
                        return DAWN_VALIDATION_ERROR(
                            "Binding already used in default pipeline layout initialization");
                    }
                    usedBindings[group].set(binding);

                    uint32_t bindingCount = bindingCounts[group];

                    bindings[group][bindingCount].binding = binding;
                    bindings[group][bindingCount].visibility = wgpu::ShaderStage::Vertex |
                                                               wgpu::ShaderStage::Fragment |
                                                               wgpu::ShaderStage::Compute;
                    bindings[group][bindingCount].type = bindingInfo.type;
                    bindings[group][bindingCount].hasDynamicOffset = false;
                    bindings[group][bindingCount].multisampled = bindingInfo.multisampled;
                    bindings[group][bindingCount].textureDimension = bindingInfo.textureDimension;
                    bindings[group][bindingCount].textureComponentType =
                        static_cast<wgpu::TextureComponentType>(bindingInfo.textureComponentType);

                    static_assert(wgpu::TextureComponentType::Float ==
                                      static_cast<wgpu::TextureComponentType>(Format::Type::Float),
                                  "");
                    static_assert(wgpu::TextureComponentType::Sint ==
                                      static_cast<wgpu::TextureComponentType>(Format::Type::Sint),
                                  "");
                    static_assert(wgpu::TextureComponentType::Uint ==
                                      static_cast<wgpu::TextureComponentType>(Format::Type::Uint),
                                  "");

                    bindingCounts[group]++;

                    bindGroupLayoutCount = std::max(bindGroupLayoutCount, group + 1);
                }
            }
        }

        std::array<BindGroupLayoutBase*, kMaxBindGroups> bindGroupLayouts = {};
        for (uint32_t group = 0; group < bindGroupLayoutCount; ++group) {
            if (bindingCounts[group] == 0) {
                continue;
            }

            BindGroupLayoutDescriptor desc = {};
            desc.bindings = bindings[group].data();
            desc.bindingCount = bindingCounts[group];

            // We should never produce a bad descriptor.
            ASSERT(!ValidateBindGroupLayoutDescriptor(device, &desc).IsError());

            BindGroupLayoutBase* bgl = nullptr;
            DAWN_TRY_ASSIGN(bgl, device->GetOrCreateBindGroupLayout(&desc));
            bindGroupLayouts[group] = bgl;
        }

        PipelineLayoutDescriptor desc = {};
        desc.bindGroupLayouts = bindGroupLayouts.data();
        desc.bindGroupLayoutCount = bindGroupLayoutCount;
        PipelineLayoutBase* pipelineLayout = device->CreatePipelineLayout(&desc);

        // These bind group layouts are created internally and referenced by the pipeline layout.
        // Release the external refcount.
        for (uint32_t group = 0; group < bindGroupLayoutCount; ++group) {
            if (bindGroupLayouts[group] != nullptr) {
                bindGroupLayouts[group]->Release();
            }
        }

        for (uint32_t moduleIndex = 0; moduleIndex < count; ++moduleIndex) {
            ASSERT(modules[moduleIndex]->IsCompatibleWithPipelineLayout(pipelineLayout));
        }

        return pipelineLayout;
    }

    const BindGroupLayoutBase* PipelineLayoutBase::GetBindGroupLayout(uint32_t group) const {
        ASSERT(!IsError());
        ASSERT(group < kMaxBindGroups);
        ASSERT(mMask[group]);
        return mBindGroupLayouts[group].Get();
    }

    BindGroupLayoutBase* PipelineLayoutBase::GetBindGroupLayout(uint32_t group) {
        ASSERT(!IsError());
        ASSERT(group < kMaxBindGroups);
        ASSERT(mMask[group]);
        return mBindGroupLayouts[group].Get();
    }

    const std::bitset<kMaxBindGroups> PipelineLayoutBase::GetBindGroupLayoutsMask() const {
        ASSERT(!IsError());
        return mMask;
    }

    std::bitset<kMaxBindGroups> PipelineLayoutBase::InheritedGroupsMask(
        const PipelineLayoutBase* other) const {
        ASSERT(!IsError());
        return {(1 << GroupsInheritUpTo(other)) - 1u};
    }

    uint32_t PipelineLayoutBase::GroupsInheritUpTo(const PipelineLayoutBase* other) const {
        ASSERT(!IsError());

        for (uint32_t i = 0; i < kMaxBindGroups; ++i) {
            if (!mMask[i] || mBindGroupLayouts[i].Get() != other->mBindGroupLayouts[i].Get()) {
                return i;
            }
        }
        return kMaxBindGroups;
    }

    size_t PipelineLayoutBase::HashFunc::operator()(const PipelineLayoutBase* pl) const {
        size_t hash = Hash(pl->mMask);

        for (uint32_t group : IterateBitSet(pl->mMask)) {
            HashCombine(&hash, pl->GetBindGroupLayout(group));
        }

        return hash;
    }

    bool PipelineLayoutBase::EqualityFunc::operator()(const PipelineLayoutBase* a,
                                                      const PipelineLayoutBase* b) const {
        if (a->mMask != b->mMask) {
            return false;
        }

        for (uint32_t group : IterateBitSet(a->mMask)) {
            if (a->GetBindGroupLayout(group) != b->GetBindGroupLayout(group)) {
                return false;
            }
        }

        return true;
    }

}  // namespace dawn_native
