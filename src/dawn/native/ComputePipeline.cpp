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

#include "dawn/native/ComputePipeline.h"

#include "dawn/native/Device.h"
#include "dawn/native/ObjectContentHasher.h"
#include "dawn/native/ObjectType_autogen.h"
#include "dawn/native/ShaderModule.h"

#include "tint/tint.h"

namespace dawn::native {

MaybeError ValidateComputePipelineDescriptor(DeviceBase* device,
                                             const ComputePipelineDescriptor* descriptor) {
    if (descriptor->nextInChain != nullptr) {
        return DAWN_FORMAT_VALIDATION_ERROR("nextInChain must be nullptr.");
    }

    if (descriptor->layout != nullptr) {
        DAWN_TRY(device->ValidateObject(descriptor->layout));
    }

    return ValidateProgrammableStage(
        device, descriptor->compute.module, descriptor->compute.entryPoint,
        descriptor->compute.constantCount, descriptor->compute.constants, descriptor->layout,
        SingleShaderStage::Compute);
}

// ComputePipelineBase

ComputePipelineBase::ComputePipelineBase(DeviceBase* device,
                                         const ComputePipelineDescriptor* descriptor)
    : PipelineBase(
          device,
          descriptor->layout,
          descriptor->label,
          {{SingleShaderStage::Compute, descriptor->compute.module, descriptor->compute.entryPoint,
            descriptor->compute.constantCount, descriptor->compute.constants}}) {
    SetContentHash(ComputeContentHash());
    TrackInDevice();

    // Initialize the cache key to include the cache type and device information.
    StreamIn(&mCacheKey, CacheKey::Type::ComputePipeline, device->GetCacheKey());
}

ComputePipelineBase::ComputePipelineBase(DeviceBase* device) : PipelineBase(device) {
    TrackInDevice();
}

ComputePipelineBase::ComputePipelineBase(DeviceBase* device, ObjectBase::ErrorTag tag)
    : PipelineBase(device, tag) {}

ComputePipelineBase::~ComputePipelineBase() = default;

void ComputePipelineBase::DestroyImpl() {
    if (IsCachedReference()) {
        // Do not uncache the actual cached object if we are a blueprint.
        GetDevice()->UncacheComputePipeline(this);
    }
}

MaybeError ComputePipelineBase::RunTintProgramTransformWorkgroupSize() {
    const ProgrammableStage& stage = GetStage(SingleShaderStage::Compute);
    const EntryPointMetadata& metadata = *stage.metadata;
    auto& constants = stage.constants;

    if (!metadata.workgroupSizeOverrides.empty()) {
        // Special handling for overrides used as workgroup size
        tint::transform::SubstituteOverride substituteOverride;

        tint::transform::SubstituteOverride::Config cfg;
        // for (auto key : metadata.workgroupSizeOverrides) {
        for (const auto& [key, value] : constants) {
            if (metadata.workgroupSizeOverrides.count(key) > 0) {
                const auto& o = metadata.overrides.at(key);
                DAWN_ASSERT(o.type == EntryPointMetadata::Override::Type::Uint32);
                cfg.map.insert({tint::OverrideId{static_cast<uint16_t>(o.id)}, value});
            }

            // // temp
            // DAWN_INVALID_IF(o.defaultValue.u32 < 1u, "workgroup_size argument must be at
            // least 1.");

            // cfg.map.insert(o.id, o.defaultValue});
            // cfg.map.insert({tint::OverrideId{static_cast<uint16_t>(o.id)}, o.defaultValue.f32});
            // cfg.map.insert({tint::OverrideId{static_cast<uint16_t>(o.id)}, stage.constants});
            // cfg.map.insert({tint::OverrideId{static_cast<uint16_t>(o.id)}, 16});
        }
        tint::transform::DataMap data;
        data.Add<tint::transform::SubstituteOverride::Config>(cfg);

        tint::Program program;
        DAWN_TRY_ASSIGN(program, RunTransforms(&substituteOverride, stage.module->GetTintProgram(),
                                               data, nullptr, nullptr));
        // stage.module->SetTintProgram(std::move(program));

        tint::inspector::Inspector inspector(&program);

        // DelayedCheckIfWorkgroupSizeIsInvalid(
        //     const_cast(metadata),
        //     GetDevice()->GetLimits(),
        //     // temp

        //     inspector.GetWorkgroupStorageSize());

        stage.module->SetTintProgram(std::make_unique<tint::Program>(std::move(program)));
    }
    return {};
}

// static
ComputePipelineBase* ComputePipelineBase::MakeError(DeviceBase* device) {
    class ErrorComputePipeline final : public ComputePipelineBase {
      public:
        explicit ErrorComputePipeline(DeviceBase* device)
            : ComputePipelineBase(device, ObjectBase::kError) {}

        MaybeError Initialize() override {
            UNREACHABLE();
            return {};
        }
    };

    return new ErrorComputePipeline(device);
}

ObjectType ComputePipelineBase::GetType() const {
    return ObjectType::ComputePipeline;
}

bool ComputePipelineBase::EqualityFunc::operator()(const ComputePipelineBase* a,
                                                   const ComputePipelineBase* b) const {
    return PipelineBase::EqualForCache(a, b);
}

}  // namespace dawn::native
