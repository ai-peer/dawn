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

#include "dawn_native/metal/ShaderModuleMTL.h"

#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/SpirvUtils.h"
#include "dawn_native/metal/DeviceMTL.h"
#include "dawn_native/metal/PipelineLayoutMTL.h"
#include "dawn_native/metal/RenderPipelineMTL.h"

#include <spirv_msl.hpp>

#ifdef DAWN_ENABLE_WGSL
// Tint include must be after spirv_msl.hpp, because spirv-cross has its own
// version of spirv_headers. We also need to undef SPV_REVISION because SPIRV-Cross
// is at 3 while spirv-headers is at 4.
#    undef SPV_REVISION
#    include <tint/tint.h>
#endif  // DAWN_ENABLE_WGSL

#include <sstream>

namespace dawn_native { namespace metal {

    namespace {
        // Use set 4 since it is bigger than what users can access currently
        static const uint32_t kPullingBufferBindingSet = 4;

#ifdef DAWN_ENABLE_WGSL
        tint::transform::VertexFormat ToTintVertexFormat(wgpu::VertexFormat format) {
            switch (format) {
                case wgpu::VertexFormat::UChar2:
                    return tint::transform::VertexFormat::kVec2U8;
                case wgpu::VertexFormat::UChar4:
                    return tint::transform::VertexFormat::kVec4U8;
                case wgpu::VertexFormat::Char2:
                    return tint::transform::VertexFormat::kVec2I8;
                case wgpu::VertexFormat::Char4:
                    return tint::transform::VertexFormat::kVec4I8;
                case wgpu::VertexFormat::UChar2Norm:
                    return tint::transform::VertexFormat::kVec2U8Norm;
                case wgpu::VertexFormat::UChar4Norm:
                    return tint::transform::VertexFormat::kVec4U8Norm;
                case wgpu::VertexFormat::Char2Norm:
                    return tint::transform::VertexFormat::kVec2I8Norm;
                case wgpu::VertexFormat::Char4Norm:
                    return tint::transform::VertexFormat::kVec4I8Norm;
                case wgpu::VertexFormat::UShort2:
                    return tint::transform::VertexFormat::kVec2U16;
                case wgpu::VertexFormat::UShort4:
                    return tint::transform::VertexFormat::kVec4U16;
                case wgpu::VertexFormat::Short2:
                    return tint::transform::VertexFormat::kVec2I16;
                case wgpu::VertexFormat::Short4:
                    return tint::transform::VertexFormat::kVec4I16;
                case wgpu::VertexFormat::UShort2Norm:
                    return tint::transform::VertexFormat::kVec2U16Norm;
                case wgpu::VertexFormat::UShort4Norm:
                    return tint::transform::VertexFormat::kVec4U16Norm;
                case wgpu::VertexFormat::Short2Norm:
                    return tint::transform::VertexFormat::kVec2I16Norm;
                case wgpu::VertexFormat::Short4Norm:
                    return tint::transform::VertexFormat::kVec4I16Norm;
                case wgpu::VertexFormat::Half2:
                    return tint::transform::VertexFormat::kVec2F16;
                case wgpu::VertexFormat::Half4:
                    return tint::transform::VertexFormat::kVec4F16;
                case wgpu::VertexFormat::Float:
                    return tint::transform::VertexFormat::kF32;
                case wgpu::VertexFormat::Float2:
                    return tint::transform::VertexFormat::kVec2F32;
                case wgpu::VertexFormat::Float3:
                    return tint::transform::VertexFormat::kVec3F32;
                case wgpu::VertexFormat::Float4:
                    return tint::transform::VertexFormat::kVec4F32;
                case wgpu::VertexFormat::UInt:
                    return tint::transform::VertexFormat::kU32;
                case wgpu::VertexFormat::UInt2:
                    return tint::transform::VertexFormat::kVec2U32;
                case wgpu::VertexFormat::UInt3:
                    return tint::transform::VertexFormat::kVec3U32;
                case wgpu::VertexFormat::UInt4:
                    return tint::transform::VertexFormat::kVec4U32;
                case wgpu::VertexFormat::Int:
                    return tint::transform::VertexFormat::kI32;
                case wgpu::VertexFormat::Int2:
                    return tint::transform::VertexFormat::kVec2I32;
                case wgpu::VertexFormat::Int3:
                    return tint::transform::VertexFormat::kVec3I32;
                case wgpu::VertexFormat::Int4:
                    return tint::transform::VertexFormat::kVec4I32;
            }
        }

        tint::transform::InputStepMode ToTintInputStepMode(wgpu::InputStepMode mode) {
            switch (mode) {
                case wgpu::InputStepMode::Vertex:
                    return tint::transform::InputStepMode::kVertex;
                case wgpu::InputStepMode::Instance:
                    return tint::transform::InputStepMode::kInstance;
            }
        }

        std::unique_ptr<tint::transform::VertexPullingTransform> MakeVertexPullingTransform(
            tint::Context* context,
            tint::ast::Module* module,
            const VertexStateDescriptor& vertexState,
            const std::string& entryPoint) {
            auto transform =
                std::make_unique<tint::transform::VertexPullingTransform>(context, module);
            auto state = std::make_unique<tint::transform::VertexStateDescriptor>();
            for (uint32_t i = 0; i < vertexState.vertexBufferCount; ++i) {
                auto& vertexBuffer = vertexState.vertexBuffers[i];
                tint::transform::VertexBufferLayoutDescriptor layout;
                layout.array_stride = vertexBuffer.arrayStride;
                layout.step_mode = ToTintInputStepMode(vertexBuffer.stepMode);

                for (uint32_t j = 0; j < vertexBuffer.attributeCount; ++j) {
                    auto& attribute = vertexBuffer.attributes[j];
                    tint::transform::VertexAttributeDescriptor attr;
                    attr.format = ToTintVertexFormat(attribute.format);
                    attr.offset = attribute.offset;
                    attr.shader_location = attribute.shaderLocation;

                    layout.attributes.push_back(std::move(attr));
                }

                state->vertex_buffers.push_back(std::move(layout));
            }
            transform->SetVertexState(std::move(state));
            transform->SetEntryPoint(entryPoint);
            transform->SetPullingBufferBindingSet(kPullingBufferBindingSet);

            return transform;
        }

        ResultOrError<std::vector<uint32_t>> GeneratePullingSpirv(
            tint::Context* context,
            tint::ast::Module&& module,
            const VertexStateDescriptor& vertexState,
            const std::string& entryPoint) {
            std::ostringstream errorStream;
            errorStream << "Tint vertex pulling failure:" << std::endl;

            {
                tint::TypeDeterminer typeDeterminer(context, &module);
                if (!typeDeterminer.Determine()) {
                    errorStream << "Type Determination: " << typeDeterminer.error();
                    return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
                }
            }

            tint::transform::Manager transformManager(context, &module);
            transformManager.append(
                MakeVertexPullingTransform(context, &module, vertexState, entryPoint));
            transformManager.append(
                std::make_unique<tint::transform::BoundArrayAccessorsTransform>(context, &module));

            if (!transformManager.Run()) {
                errorStream << "Vertex pulling + bound array acccessors tranforms: "
                            << transformManager.error();
                return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
            }

            {
                tint::TypeDeterminer typeDeterminer(context, &module);
                if (!typeDeterminer.Determine()) {
                    errorStream << "Type Determination: " << typeDeterminer.error();
                    return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
                }
            }

            tint::writer::spirv::Generator generator(std::move(module));
            if (!generator.Generate()) {
                errorStream << "Generator: " << generator.error() << std::endl;
                return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
            }

            std::vector<uint32_t> spirv = generator.result();
            DAWN_TRY(ValidateSpirv(spirv.data(), spirv.size()));
            return std::move(spirv);
        }

        ResultOrError<std::vector<uint32_t>> GeneratePullingSpirv(
            const std::vector<uint32_t>& spirv,
            const VertexStateDescriptor& vertexState,
            const std::string& entryPoint) {
            tint::Context context;
            tint::ast::Module module;
            DAWN_TRY_ASSIGN(module, ParseSPIRV(&context, spirv));

            return GeneratePullingSpirv(&context, std::move(module), vertexState, entryPoint);
        }

#endif  // DAWN_ENABLE_WGSL
    }

    // static
    ResultOrError<ShaderModule*> ShaderModule::Create(Device* device,
                                                      const ShaderModuleDescriptor* descriptor,
                                                      ShaderModuleParseResult* parseResult) {
        Ref<ShaderModule> module = AcquireRef(new ShaderModule(device, descriptor));
        DAWN_TRY(module->Initialize(parseResult));
        return module.Detach();
    }

    ShaderModule::ShaderModule(Device* device, const ShaderModuleDescriptor* descriptor)
        : ShaderModuleBase(device, descriptor) {
    }

    MaybeError ShaderModule::Initialize(ShaderModuleParseResult* parseResult) {
        DAWN_TRY(InitializeBase(parseResult));
#ifdef DAWN_ENABLE_WGSL
        mTintContext = std::move(parseResult->tintContext);
        mTintModule = std::move(parseResult->tintModule);
#endif
        return {};
    }

    ResultOrError<std::string> ShaderModule::TranslateToMSLWithTint(
        const char* entryPointName,
        SingleShaderStage stage,
        const PipelineLayout* layout,
        // TODO(crbug.com/tint/387): AND in a fixed sample mask in the shader.
        uint32_t sampleMask,
        const RenderPipeline* renderPipeline,
        std::string* remappedEntryPointName,
        bool* needsStorageBufferLength) {
#if DAWN_ENABLE_WGSL
        // TODO(crbug.com/tint/256): Set this accordingly if arrayLength(..) is used.
        *needsStorageBufferLength = false;

        std::ostringstream errorStream;
        errorStream << "Tint MSL failure:" << std::endl;

        tint::Context* context = mTintContext.get();
        tint::ast::Module module = mTintModule->Clone();

        {
            tint::TypeDeterminer typeDeterminer(context, &module);
            if (!typeDeterminer.Determine()) {
                errorStream << "Type Determination: " << typeDeterminer.error();
                return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
            }
        }

        tint::transform::Manager transformManager(context, &module);
        if (stage == SingleShaderStage::Vertex &&
            GetDevice()->IsToggleEnabled(Toggle::MetalEnableVertexPulling)) {
            transformManager.append(MakeVertexPullingTransform(
                context, &module, *renderPipeline->GetVertexStateDescriptor(), entryPointName));

            for (VertexBufferSlot slot :
                 IterateBitSet(renderPipeline->GetVertexBufferSlotsUsed())) {
                uint32_t metalIndex = renderPipeline->GetMtlVertexBufferIndex(slot);
                DAWN_UNUSED(metalIndex);
                // TODO(crbug.com/tint/104): Tell Tint to map (kPullingBufferBindingSet, slot) to
                // this MSL buffer index.
            }
        }
        transformManager.append(
            std::make_unique<tint::transform::BoundArrayAccessorsTransform>(context, &module));
        if (!transformManager.Run()) {
            errorStream << "Bound Array Accessors Transform: " << transformManager.error()
                        << std::endl;
            return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
        }

        {
            tint::TypeDeterminer typeDeterminer(context, &module);
            if (!typeDeterminer.Determine()) {
                errorStream << "Type Determination: " << typeDeterminer.error();
                return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
            }
        }

        ASSERT(remappedEntryPointName != nullptr);
        tint::inspector::Inspector inspector(module);
        *remappedEntryPointName = inspector.GetRemappedNameForEntryPoint(entryPointName);

        tint::writer::msl::Generator generator(std::move(module));
        if (!generator.Generate()) {
            errorStream << "Generator: " << generator.error() << std::endl;
            return DAWN_VALIDATION_ERROR(errorStream.str().c_str());
        }

        std::string msl = generator.result();
        return std::move(msl);
#else
        UNREACHABLE();
#endif
    }

    ResultOrError<std::string> ShaderModule::TranslateToMSLWithSPIRVCross(
        const char* entryPointName,
        SingleShaderStage stage,
        const PipelineLayout* layout,
        uint32_t sampleMask,
        const RenderPipeline* renderPipeline,
        std::string* remappedEntryPointName,
        bool* needsStorageBufferLength) {
        const std::vector<uint32_t>* spirv = &GetSpirv();
        spv::ExecutionModel executionModel = ShaderStageToExecutionModel(stage);

#ifdef DAWN_ENABLE_WGSL
        std::vector<uint32_t> pullingSpirv;
        if (GetDevice()->IsToggleEnabled(Toggle::MetalEnableVertexPulling) &&
            stage == SingleShaderStage::Vertex) {
            if (GetDevice()->IsToggleEnabled(Toggle::UseTintGenerator)) {
                DAWN_TRY_ASSIGN(pullingSpirv,
                                GeneratePullingSpirv(mTintContext.get(), mTintModule->Clone(),
                                                     *renderPipeline->GetVertexStateDescriptor(),
                                                     entryPointName));
            } else {
                DAWN_TRY_ASSIGN(
                    pullingSpirv,
                    GeneratePullingSpirv(GetSpirv(), *renderPipeline->GetVertexStateDescriptor(),
                                         entryPointName));
            }
            spirv = &pullingSpirv;
        }
#endif

        // If these options are changed, the values in DawnSPIRVCrossMSLFastFuzzer.cpp need to
        // be updated.
        spirv_cross::CompilerMSL::Options options_msl;

        // Disable PointSize builtin for https://bugs.chromium.org/p/dawn/issues/detail?id=146
        // Because Metal will reject PointSize builtin if the shader is compiled into a render
        // pipeline that uses a non-point topology.
        // TODO (hao.x.li@intel.com): Remove this once WebGPU requires there is no
        // gl_PointSize builtin (https://github.com/gpuweb/gpuweb/issues/332).
        options_msl.enable_point_size_builtin = false;

        // Always use vertex buffer 30 (the last one in the vertex buffer table) to contain
        // the shader storage buffer lengths.
        options_msl.buffer_size_buffer_index = kBufferLengthBufferSlot;

        options_msl.additional_fixed_sample_mask = sampleMask;

        spirv_cross::CompilerMSL compiler(*spirv);
        compiler.set_msl_options(options_msl);
        compiler.set_entry_point(entryPointName, executionModel);

        // By default SPIRV-Cross will give MSL resources indices in increasing order.
        // To make the MSL indices match the indices chosen in the PipelineLayout, we build
        // a table of MSLResourceBinding to give to SPIRV-Cross.

        // Create one resource binding entry per stage per binding.
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

                uint32_t shaderIndex = layout->GetBindingIndexInfo(stage)[group][bindingIndex];

                spirv_cross::MSLResourceBinding mslBinding;
                mslBinding.stage = executionModel;
                mslBinding.desc_set = static_cast<uint32_t>(group);
                mslBinding.binding = static_cast<uint32_t>(bindingNumber);
                mslBinding.msl_buffer = mslBinding.msl_texture = mslBinding.msl_sampler =
                    shaderIndex;

                compiler.add_msl_resource_binding(mslBinding);
            }
        }

#ifdef DAWN_ENABLE_WGSL
        // Add vertex buffers bound as storage buffers
        if (GetDevice()->IsToggleEnabled(Toggle::MetalEnableVertexPulling) &&
            stage == SingleShaderStage::Vertex) {
            for (VertexBufferSlot slot :
                 IterateBitSet(renderPipeline->GetVertexBufferSlotsUsed())) {
                uint32_t metalIndex = renderPipeline->GetMtlVertexBufferIndex(slot);

                spirv_cross::MSLResourceBinding mslBinding;

                mslBinding.stage = spv::ExecutionModelVertex;
                mslBinding.desc_set = kPullingBufferBindingSet;
                mslBinding.binding = static_cast<uint8_t>(slot);
                mslBinding.msl_buffer = metalIndex;
                compiler.add_msl_resource_binding(mslBinding);
            }
        }
#endif

        // SPIRV-Cross also supports re-ordering attributes but it seems to do the correct thing
        // by default.
        std::string msl = compiler.compile();

        // Some entry point names are forbidden in MSL so SPIRV-Cross modifies them. Query the
        // modified entryPointName from it.
        *remappedEntryPointName = compiler.get_entry_point(entryPointName, executionModel).name;
        *needsStorageBufferLength = compiler.needs_buffer_size_buffer();

        return std::move(msl);
    }

    MaybeError ShaderModule::CreateFunction(const char* entryPointName,
                                            SingleShaderStage stage,
                                            const PipelineLayout* layout,
                                            ShaderModule::MetalFunctionData* out,
                                            uint32_t sampleMask,
                                            const RenderPipeline* renderPipeline) {
        ASSERT(!IsError());
        ASSERT(out);

        std::string remappedEntryPointName;
        std::string msl;
        if (GetDevice()->IsToggleEnabled(Toggle::UseTintGenerator)) {
            DAWN_TRY_ASSIGN(msl, TranslateToMSLWithTint(entryPointName, stage, layout, sampleMask,
                                                        renderPipeline, &remappedEntryPointName,
                                                        &out->needsStorageBufferLength));
        } else {
            DAWN_TRY_ASSIGN(msl, TranslateToMSLWithSPIRVCross(
                                     entryPointName, stage, layout, sampleMask, renderPipeline,
                                     &remappedEntryPointName, &out->needsStorageBufferLength));
        }

        // Metal uses Clang to compile the shader as C++14. Disable everything in the -Wall
        // category. -Wunused-variable in particular comes up a lot in generated code, and some
        // (old?) Metal drivers accidentally treat it as a MTLLibraryErrorCompileError instead
        // of a warning.
        msl = R"(\
#ifdef __clang__
#pragma clang diagnostic ignored "-Wall"
#endif
)" + msl;

        NSRef<NSString> mslSource = AcquireNSRef([[NSString alloc] initWithUTF8String:msl.c_str()]);

        auto mtlDevice = ToBackend(GetDevice())->GetMTLDevice();
        NSError* error = nullptr;
        NSPRef<id<MTLLibrary>> library =
            AcquireNSPRef([mtlDevice newLibraryWithSource:mslSource.Get()
                                                  options:nullptr
                                                    error:&error]);
        if (error != nullptr) {
            if (error.code != MTLLibraryErrorCompileWarning) {
                const char* errorString = [error.localizedDescription UTF8String];
                return DAWN_VALIDATION_ERROR(std::string("Unable to create library object: ") +
                                             errorString);
            }
        }

        NSRef<NSString> name =
            AcquireNSRef([[NSString alloc] initWithUTF8String:remappedEntryPointName.c_str()]);
        out->function = AcquireNSPRef([*library newFunctionWithName:name.Get()]);

        if (GetDevice()->IsToggleEnabled(Toggle::MetalEnableVertexPulling) &&
            GetEntryPoint(entryPointName).usedVertexAttributes.any()) {
            out->needsStorageBufferLength = true;
        }

        return {};
    }
}}  // namespace dawn_native::metal
