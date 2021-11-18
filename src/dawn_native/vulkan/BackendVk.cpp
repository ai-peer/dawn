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

#include "dawn_native/vulkan/BackendVk.h"

#include "dawn_native/Instance.h"
#include "dawn_native/vulkan/AdapterVk.h"

namespace dawn_native { namespace vulkan {

    namespace {

        static constexpr ICD kICDs[] = {
            ICD::None,
#if defined(DAWN_ENABLE_SWIFTSHADER)
            ICD::SwiftShader,
#endif  // defined(DAWN_ENABLE_SWIFTSHADER)
        };

    }  // anonymous namespace

    Backend::Backend(InstanceBase* instance)
        : BackendConnection(instance, wgpu::BackendType::Vulkan) {
    }

    Backend::~Backend() = default;

    std::vector<std::unique_ptr<AdapterBase>> Backend::DiscoverDefaultAdapters() {
        std::vector<std::unique_ptr<AdapterBase>> adapters;

        InstanceBase* instance = GetInstance();
        for (ICD icd : kICDs) {
            if (mVulkanInstances[icd] == nullptr && instance->ConsumedError([&]() -> MaybeError {
                    DAWN_TRY_ASSIGN(mVulkanInstances[icd], VulkanInstance::Create(instance, icd));
                    return {};
                }())) {
                // Instance failed to initialize.
                continue;
            }
            const std::vector<VkPhysicalDevice>& physicalDevices =
                mVulkanInstances[icd]->GetPhysicalDevices();
            for (uint32_t i = 0; i < physicalDevices.size(); ++i) {
                std::unique_ptr<Adapter> adapter = std::make_unique<Adapter>(
                    instance, mVulkanInstances[icd].Get(), physicalDevices[i]);
                if (instance->ConsumedError(adapter->Initialize())) {
                    continue;
                }
                adapters.push_back(std::move(adapter));
            }
            return {};
        }

        return adapters;
    }

    BackendConnection* Connect(InstanceBase* instance) {
        return new Backend(instance);
    }

}}  // namespace dawn_native::vulkan
