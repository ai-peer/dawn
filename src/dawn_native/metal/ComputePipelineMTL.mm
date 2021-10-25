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

#include "dawn_native/metal/ComputePipelineMTL.h"

#include "dawn_native/CreatePipelineAsyncTask.h"
#include "dawn_native/metal/DeviceMTL.h"
#include "dawn_native/metal/ShaderModuleMTL.h"

#include <unordered_set>

namespace dawn_native { namespace metal {

    // static
    Ref<ComputePipeline> ComputePipeline::CreateUninitialized(
        Device* device,
        const ComputePipelineDescriptor* descriptor) {
        return AcquireRef(new ComputePipeline(device, descriptor));
    }

    MaybeError ComputePipeline::Initialize() {
        auto mtlDevice = ToBackend(GetDevice())->GetMTLDevice();

        const ProgrammableStage& computeStage = GetStage(SingleShaderStage::Compute);
        ShaderModule* computeModule = ToBackend(computeStage.module.Get());
        const char* computeEntryPoint = computeStage.entryPoint.c_str();
        ShaderModule::MetalFunctionData computeData;



        const auto& entryPointMetaData =
            computeStage.module->GetEntryPoint(computeEntryPoint);

        if (@available(macOS 10.12, *)) {
            NSRef<MTLFunctionConstantValues> constantValues = AcquireNSRef([MTLFunctionConstantValues new]);
            // computeData.mtlConstantValues = AcquireNSRef([MTLFunctionConstantValues new]);

            std::unordered_set<std::string> overrided;

            for (const auto& pipelineConstant : computeStage.constants) {
                const std::string& name = pipelineConstant.first;
                double value = pipelineConstant.second;

                overrided.insert(name);

                // This is already validated so `name` must exist
                const auto& moduleConstant = entryPointMetaData.overridableConstants.at(name);

                MTLDataType type;
                OverridableConstantScalar entry{};
                switch (moduleConstant.type) {
                    case EntryPointMetadata::OverridableConstant::Type::Boolean:
                        type = MTLDataTypeBool;
                        entry.b = static_cast<int32_t>(value);
                        break;
                    case EntryPointMetadata::OverridableConstant::Type::Float32:
                        type = MTLDataTypeFloat;
                        entry.f32 = static_cast<float>(value);
                        break;
                    case EntryPointMetadata::OverridableConstant::Type::Int32:
                        type = MTLDataTypeInt;
                        entry.i32 = static_cast<int32_t>(value);
                        break;
                    case EntryPointMetadata::OverridableConstant::Type::Uint32:
                        type = MTLDataTypeUInt;
                        entry.u32 = static_cast<uint32_t>(value);
                        break;
                    default:
                        UNREACHABLE();
                }

                printf("%s %f\n", name.c_str(), value);

                // TODO: value cast

                // temp uint
                // [constantValues setConstantValue:value type:type atIndex:moduleConstant.id];
                [constantValues.Get() setConstantValue:&entry type:type atIndex:moduleConstant.id];
                // [computeData.mtlConstantValues.Get() setConstantValue:&value type:type atIndex:moduleConstant.id];

                // setConstantValue
                // newFunctionWithName:constantValues:completionHandler:
                // need to specialize each function?
            }

            // Set shader initialized default values
            // TEMP iterator
            for(const auto & shaderConstant : entryPointMetaData.overridableConstants) {
                const std::string& name = shaderConstant.first;
                const auto& moduleConstant = shaderConstant.second;

                if (overrided.count(name) == 0 && moduleConstant.isInitialized) {
                    // printf("%s\n", name.c_str());
                    MTLDataType type;
                    // OverridableConstantScalar entry{};
                    // OverridableConstantScalar entry = moduleConstant.defaultValue;
                    switch (moduleConstant.type) {
                        case EntryPointMetadata::OverridableConstant::Type::Boolean:
                            type = MTLDataTypeBool;
                            printf("---\n%s %d\n", name.c_str(), moduleConstant.defaultValue.b);
                            break;
                        case EntryPointMetadata::OverridableConstant::Type::Float32:
                            type = MTLDataTypeFloat;
                            // entry.f32 = moduleConstant.AsFloat();
                            printf("---\n%s %f\n", name.c_str(), moduleConstant.defaultValue.f32);
                            break;
                        case EntryPointMetadata::OverridableConstant::Type::Int32:
                            type = MTLDataTypeInt;
                            // entry.i32 = moduleConstant.AsI32();
                            printf("---\n%s %d\n", name.c_str(), moduleConstant.defaultValue.i32);
                            break;
                        case EntryPointMetadata::OverridableConstant::Type::Uint32:
                            type = MTLDataTypeUInt;
                            // entry.u32 = moduleConstant.AsU32();
                            printf("---\n%s %u\n", name.c_str(), moduleConstant.defaultValue.u32);
                            break;
                        default:
                            UNREACHABLE();
                    }

                    [constantValues.Get() setConstantValue:&moduleConstant.defaultValue type:type atIndex:moduleConstant.id];
                    // [constantValues.Get() setConstantValue:&entry type:type atIndex:moduleConstant.id];
                }
            }

            // NSRef<id> constantValuesPointer = AcquireNSRef(*constantValues.Get());
            // NSPRef<id> constantValuesPointer = AcquireNSPRef(constantValues.Get());
            id constantValuesPointer = constantValues.Get();
            DAWN_TRY(computeModule->CreateFunction(computeEntryPoint, SingleShaderStage::Compute,
                                               ToBackend(GetLayout()), &computeData, 
                                               constantValuesPointer
                                               ));


        }
        else
        {
            // TODO: assert constant entries are empty
            DAWN_TRY(computeModule->CreateFunction(computeEntryPoint, SingleShaderStage::Compute,
                                               ToBackend(GetLayout()), &computeData, nil));
        }


        // DAWN_TRY(computeModule->CreateFunction(computeEntryPoint, SingleShaderStage::Compute,
        //                                        ToBackend(GetLayout()), &computeData));

        NSError* error = nullptr;
        mMtlComputePipelineState.Acquire([mtlDevice
            newComputePipelineStateWithFunction:computeData.function.Get()
                                          error:&error]);
        if (error != nullptr) {
            return DAWN_INTERNAL_ERROR("Error creating pipeline state" +
                                       std::string([error.localizedDescription UTF8String]));
        }
        ASSERT(mMtlComputePipelineState != nil);

        // Copy over the local workgroup size as it is passed to dispatch explicitly in Metal
        Origin3D localSize = GetStage(SingleShaderStage::Compute).metadata->localWorkgroupSize;
        mLocalWorkgroupSize = MTLSizeMake(localSize.x, localSize.y, localSize.z);

        mRequiresStorageBufferLength = computeData.needsStorageBufferLength;
        mWorkgroupAllocations = std::move(computeData.workgroupAllocations);
        return {};
    }

    void ComputePipeline::Encode(id<MTLComputeCommandEncoder> encoder) {
        [encoder setComputePipelineState:mMtlComputePipelineState.Get()];
        for (size_t i = 0; i < mWorkgroupAllocations.size(); ++i) {
            if (mWorkgroupAllocations[i] == 0) {
                continue;
            }
            [encoder setThreadgroupMemoryLength:mWorkgroupAllocations[i] atIndex:i];
        }
    }

    MTLSize ComputePipeline::GetLocalWorkGroupSize() const {
        return mLocalWorkgroupSize;
    }

    bool ComputePipeline::RequiresStorageBufferLength() const {
        return mRequiresStorageBufferLength;
    }

    void ComputePipeline::InitializeAsync(Ref<ComputePipelineBase> computePipeline,
                                          WGPUCreateComputePipelineAsyncCallback callback,
                                          void* userdata) {
        std::unique_ptr<CreateComputePipelineAsyncTask> asyncTask =
            std::make_unique<CreateComputePipelineAsyncTask>(std::move(computePipeline), callback,
                                                             userdata);
        CreateComputePipelineAsyncTask::RunAsync(std::move(asyncTask));
    }

}}  // namespace dawn_native::metal
