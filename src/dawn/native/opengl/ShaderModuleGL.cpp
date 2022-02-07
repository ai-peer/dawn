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

#include "dawn/native/opengl/ShaderModuleGL.h"

#include "dawn/native/BindGroupLayout.h"
#include "dawn/native/TintUtils.h"
#include "dawn/native/opengl/DeviceGL.h"
#include "dawn/native/opengl/PipelineLayoutGL.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

#include <tint/tint.h>

#include <sstream>

namespace dawn::native::opengl {

    std::string GetBindingName(BindGroupIndex group, BindingNumber bindingNumber) {
        std::ostringstream o;
        o << "dawn_binding_" << static_cast<uint32_t>(group) << "_"
          << static_cast<uint32_t>(bindingNumber);
        return o.str();
    }

    bool operator<(const BindingLocation& a, const BindingLocation& b) {
        return std::tie(a.group, a.binding) < std::tie(b.group, b.binding);
    }

    bool operator<(const CombinedSampler& a, const CombinedSampler& b) {
        return std::tie(a.useDummySampler, a.samplerLocation, a.textureLocation) <
               std::tie(b.useDummySampler, a.samplerLocation, b.textureLocation);
    }

    std::string CombinedSampler::GetName() const {
        std::ostringstream o;
        o << "dawn_combined";
        if (useDummySampler) {
            o << "_dummy_sampler";
        } else {
            o << "_" << static_cast<uint32_t>(samplerLocation.group) << "_"
              << static_cast<uint32_t>(samplerLocation.binding);
        }
        o << "_with_" << static_cast<uint32_t>(textureLocation.group) << "_"
          << static_cast<uint32_t>(textureLocation.binding);
        return o.str();
    }

    // static
    ResultOrError<Ref<ShaderModule>> ShaderModule::Create(Device* device,
                                                          const ShaderModuleDescriptor* descriptor,
                                                          ShaderModuleParseResult* parseResult) {
        Ref<ShaderModule> module = AcquireRef(new ShaderModule(device, descriptor));
        DAWN_TRY(module->Initialize(parseResult));
        return module;
    }

    ShaderModule::ShaderModule(Device* device, const ShaderModuleDescriptor* descriptor)
        : ShaderModuleBase(device, descriptor) {
    }

    MaybeError ShaderModule::Initialize(ShaderModuleParseResult* parseResult) {
        ScopedTintICEHandler scopedICEHandler(GetDevice());

        DAWN_TRY(InitializeBase(parseResult));

        return {};
    }

    ResultOrError<std::string> ShaderModule::TranslateToGLSL(const char* entryPointName,
                                                             SingleShaderStage stage,
                                                             CombinedSamplerInfo* combinedSamplers,
                                                             const PipelineLayout* layout,
                                                             bool* needsDummySampler) const {
        TRACE_EVENT0(GetDevice()->GetPlatform(), General, "TranslateToGLSL");
        const OpenGLFunctions& gl = ToBackend(GetDevice())->gl;
        tint::writer::glsl::Options tintOptions;
        tintOptions.platform = gl.GetVersion().IsDesktop()
                                   ? tint::writer::glsl::Options::Platform::kDesktopGL
                                   : tint::writer::glsl::Options::Platform::kGLES;

        tint::transform::Manager transformManager;
        tint::transform::DataMap transformInputs;
        tint::transform::DataMap transformOutputs;

        transformManager.Add<tint::transform::Renamer>();

        transformInputs.Add<tint::transform::Renamer::Config>(
            tint::transform::Renamer::Target::kGlslKeywords);

        tint::Program program;
        DAWN_TRY_ASSIGN(program, RunTransforms(&transformManager, GetTintProgram(), transformInputs,
                                               &transformOutputs, nullptr));

        std::string remappedEntryPointName = entryPointName;
        if (auto* data = transformOutputs.Get<tint::transform::Renamer::Data>()) {
            auto it = data->remappings.find(entryPointName);
            if (it != data->remappings.end()) {
                remappedEntryPointName = it->second;
            }
        } else {
            return DAWN_FORMAT_VALIDATION_ERROR("Transform output missing renamer data.");
        }
        tint::inspector::Inspector inspector(&program);
        tint::sem::BindingPoint placeholderBindingPoint{static_cast<uint32_t>(kMaxBindGroupsTyped),
                                                        0};
        for (auto use :
             inspector.GetSamplerTextureUses(remappedEntryPointName, placeholderBindingPoint)) {
            combinedSamplers->emplace_back();

            CombinedSampler* info = &combinedSamplers->back();
            if (use.sampler_binding_point == placeholderBindingPoint) {
                info->useDummySampler = true;
                *needsDummySampler = true;
            } else {
                info->useDummySampler = false;
            }
            info->samplerLocation.group = BindGroupIndex(use.sampler_binding_point.group);
            info->samplerLocation.binding = BindingNumber(use.sampler_binding_point.binding);
            info->textureLocation.group = BindGroupIndex(use.texture_binding_point.group);
            info->textureLocation.binding = BindingNumber(use.texture_binding_point.binding);
            tintOptions.binding_map[use] = info->GetName();
        }
        if (*needsDummySampler) {
            tintOptions.placeholder_binding_point = placeholderBindingPoint;
        }
        using tint::transform::BindingPoint;
        using tint::transform::BindingRemapper;

        for (BindGroupIndex group : IterateBitSet(layout->GetBindGroupLayoutsMask())) {
            const BindGroupLayoutBase::BindingMap& bindingMap =
                layout->GetBindGroupLayout(group)->GetBindingMap();
            for (const auto& it : bindingMap) {
                BindingNumber bindingNumber = it.first;
                BindingIndex bindingIndex = it.second;
                const BindingInfo& bindingInfo =
                    layout->GetBindGroupLayout(group)->GetBindingInfo(bindingIndex);
                if (!(bindingInfo.visibility & StageBit(stage))) {
                    continue;
                }

                uint32_t shaderIndex = layout->GetBindingIndexInfo()[group][bindingIndex];
                BindingPoint srcBindingPoint{static_cast<uint32_t>(group),
                                             static_cast<uint32_t>(bindingNumber)};
                BindingPoint dstBindingPoint{0, shaderIndex};
                if (srcBindingPoint != dstBindingPoint) {
                    tintOptions.binding_points.emplace(srcBindingPoint, dstBindingPoint);
                }
            }
            tintOptions.allow_collisions = true;
        }
        auto result = tint::writer::glsl::Generate(&program, tintOptions, remappedEntryPointName);
        DAWN_INVALID_IF(!result.success, "An error occured while generating GLSL: %s.",
                        result.error);
        std::string glsl = result.glsl;

        if (GetDevice()->IsToggleEnabled(Toggle::DumpShaders)) {
            std::ostringstream dumpedMsg;
            dumpedMsg << "/* Dumped generated GLSL */" << std::endl << glsl;

            GetDevice()->EmitLog(WGPULoggingType_Info, dumpedMsg.str().c_str());
        }

        return glsl;
    }

}  // namespace dawn::native::opengl
