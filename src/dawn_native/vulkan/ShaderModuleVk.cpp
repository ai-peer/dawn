// Copyright 2018 The Dawn Authors
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

#include "dawn_native/vulkan/ShaderModuleVk.h"

#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/FencedDeleter.h"
#include "dawn_native/vulkan/VulkanError.h"

#include <spirv_cross.hpp>

#ifdef DAWN_ENABLE_WGSL
// Tint include must be after spirv_cross.hpp, because spirv-cross has its own
// version of spirv_headers.
#    include <tint/tint.h>
#endif  // DAWN_ENABLE_WGSL

#include <sstream>

namespace dawn_native { namespace vulkan {

    // static
    ResultOrError<ShaderModule*> ShaderModule::Create(Device* device,
                                                      const ShaderModuleDescriptor* descriptor) {
        Ref<ShaderModule> module = AcquireRef(new ShaderModule(device, descriptor));
        if (!module)
            return DAWN_VALIDATION_ERROR("Unable to create ShaderModule");
        DAWN_TRY(module->Initialize());
        return module.Detach();
    }

    ShaderModule::ShaderModule(Device* device, const ShaderModuleDescriptor* descriptor)
        : ShaderModuleBase(device, descriptor) {
    }

    MaybeError ShaderModule::Initialize() {
        DAWN_TRY(InitializeBase());

        std::vector<uint32_t> generatedSpirv;
        const std::vector<uint32_t>* spirv;
        if (GetDevice()->IsToggleEnabled(Toggle::UseTintGenerator)) {
#ifdef DAWN_ENABLE_WGSL
            tint::Context context;
            // TODO(crbug.com/tint/306): Use DAWN_TRY_ASSIGN
            auto result = InitializeModule(&context);
            if (result.IsError()) {
                DAWN_TRY(std::move(result));
            }
            tint::ast::Module module = std::move(result.AcquireSuccess());

            std::ostringstream errorStream;
            errorStream << "Tint SPIR-V failure:" << std::endl;

            tint::writer::spirv::Generator generator(std::move(module));
            if (!generator.Generate()) {
                errorStream << "Generator: " << generator.error() << std::endl;
                return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
            }

            generatedSpirv = generator.result();

            // TODO(crbug.com/dawn/571): Eventually, this should never produce invalid SPIR-V,
            // but that's true right now. Throw a validation error so we can run and triage test
            // failures without crashing. Most end2end tests fail, but it's unclear if this is a
            // SPIR-V frontend problem or a SPIR-V backend problem.
            DAWN_TRY(ValidateSpirv(generatedSpirv.data(), generatedSpirv.size()));

            spirv = &generatedSpirv;
#else
            return DAWN_VALIDATION_ERROR("Using Tint to generate SPIR-V is not supported.");
#endif
        } else {
            spirv = &GetSpirv();
        }

        VkShaderModuleCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        std::vector<uint32_t> vulkanSource;
        createInfo.codeSize = spirv->size() * sizeof(uint32_t);
        createInfo.pCode = spirv->data();

        Device* device = ToBackend(GetDevice());
        return CheckVkSuccess(
            device->fn.CreateShaderModule(device->GetVkDevice(), &createInfo, nullptr, &*mHandle),
            "CreateShaderModule");
    }

    ShaderModule::~ShaderModule() {
        Device* device = ToBackend(GetDevice());

        if (mHandle != VK_NULL_HANDLE) {
            device->GetFencedDeleter()->DeleteWhenUnused(mHandle);
            mHandle = VK_NULL_HANDLE;
        }
    }

    VkShaderModule ShaderModule::GetHandle() const {
        return mHandle;
    }

}}  // namespace dawn_native::vulkan
