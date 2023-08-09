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

#include "dawn/native/opengl/PipelineGL.h"

#include <set>
#include <sstream>
#include <string>

#include "dawn/common/BitSetIterator.h"
#include "dawn/native/BindGroupLayout.h"
#include "dawn/native/Device.h"
#include "dawn/native/Pipeline.h"
#include "dawn/native/opengl/BufferGL.h"
#include "dawn/native/opengl/Forward.h"
#include "dawn/native/opengl/OpenGLFunctions.h"
#include "dawn/native/opengl/PipelineLayoutGL.h"
#include "dawn/native/opengl/SamplerGL.h"
#include "dawn/native/opengl/ShaderModuleGL.h"
#include "dawn/native/opengl/TextureGL.h"

namespace dawn::native::opengl {

PipelineGL::PipelineGL() : mProgram(0) {}

PipelineGL::~PipelineGL() = default;

MaybeError PipelineGL::InitializeBase(const OpenGLFunctions& gl,
                                      const PipelineLayout* layout,
                                      const PerStage<ProgrammableStage>& stages) {
    mProgram = gl.CreateProgram();

    // Compute the set of active stages.
    wgpu::ShaderStage activeStages = wgpu::ShaderStage::None;
    for (SingleShaderStage stage : IterateStages(kAllStages)) {
        if (stages[stage].module != nullptr) {
            activeStages |= StageBit(stage);
        }
    }

    // Create an OpenGL shader for each stage and gather the list of combined samplers.
    PerStage<CombinedSamplerInfo> combinedSamplers;
    bool needsPlaceholderSampler = false;
    // mNeedsTextureBuiltinUniformBuffer = false;
    std::vector<GLuint> glShaders;
    for (SingleShaderStage stage : IterateStages(activeStages)) {
        const ShaderModule* module = ToBackend(stages[stage].module.Get());
        GLuint shader;
        DAWN_TRY_ASSIGN(shader, module->CompileShader(
                                    gl, stages[stage], stage, &combinedSamplers[stage], layout,
                                    &needsPlaceholderSampler, &mNeedsTextureBuiltinUniformBuffer,
                                    &mBindingPointBuiltinsDataInfo));
        gl.AttachShader(mProgram, shader);
        glShaders.push_back(shader);
    }

    if (needsPlaceholderSampler) {
        SamplerDescriptor desc = {};
        ASSERT(desc.minFilter == wgpu::FilterMode::Nearest);
        ASSERT(desc.magFilter == wgpu::FilterMode::Nearest);
        ASSERT(desc.mipmapFilter == wgpu::MipmapFilterMode::Nearest);
        mPlaceholderSampler =
            ToBackend(layout->GetDevice()->GetOrCreateSampler(&desc).AcquireSuccess());
    }

    // if (mNeedsTextureBuiltinUniformBuffer) {
    if (!mBindingPointBuiltinsDataInfo.empty()) {
        BufferDescriptor desc = {};
        desc.size = mBindingPointBuiltinsDataInfo.size() * sizeof(uint32_t);
        desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        mTextureBuiltinsBuffer =
            ToBackend(layout->GetDevice()->CreateBuffer(&desc).AcquireSuccess());
    }

    // Link all the shaders together.
    gl.LinkProgram(mProgram);

    GLint linkStatus = GL_FALSE;
    gl.GetProgramiv(mProgram, GL_LINK_STATUS, &linkStatus);
    if (linkStatus == GL_FALSE) {
        GLint infoLogLength = 0;
        gl.GetProgramiv(mProgram, GL_INFO_LOG_LENGTH, &infoLogLength);

        if (infoLogLength > 1) {
            std::vector<char> buffer(infoLogLength);
            gl.GetProgramInfoLog(mProgram, infoLogLength, nullptr, &buffer[0]);
            return DAWN_VALIDATION_ERROR("Program link failed:\n%s", buffer.data());
        }
    }

    // Compute links between stages for combined samplers, then bind them to texture units
    gl.UseProgram(mProgram);
    const auto& indices = layout->GetBindingIndexInfo();

    std::set<CombinedSampler> combinedSamplersSet;
    for (SingleShaderStage stage : IterateStages(activeStages)) {
        for (const CombinedSampler& combined : combinedSamplers[stage]) {
            combinedSamplersSet.insert(combined);
        }
    }

    // also make a BindingLocation(group, binding) -> unitsForTexture map
    // to let TextureBuiltinsFromUniform to retrieve.

    mUnitsForSamplers.resize(layout->GetNumSamplers());
    mUnitsForTextures.resize(layout->GetNumSampledTextures());

    GLuint textureUnit = layout->GetTextureUnitsUsed();
    for (const auto& combined : combinedSamplersSet) {
        const std::string& name = combined.GetName();
        GLint location = gl.GetUniformLocation(mProgram, name.c_str());

        if (location == -1) {
            continue;
        }

        gl.Uniform1i(location, textureUnit);

        bool shouldUseFiltering;
        {
            const BindGroupLayoutBase* bgl =
                layout->GetBindGroupLayout(combined.textureLocation.group);
            BindingIndex bindingIndex = bgl->GetBindingIndex(combined.textureLocation.binding);

            GLuint textureIndex = indices[combined.textureLocation.group][bindingIndex];
            mUnitsForTextures[textureIndex].push_back(textureUnit);

            shouldUseFiltering = bgl->GetBindingInfo(bindingIndex).texture.sampleType ==
                                 wgpu::TextureSampleType::Float;
        }
        {
            if (combined.usePlaceholderSampler) {
                mPlaceholderSamplerUnits.push_back(textureUnit);
            } else {
                const BindGroupLayoutBase* bgl =
                    layout->GetBindGroupLayout(combined.samplerLocation.group);
                BindingIndex bindingIndex = bgl->GetBindingIndex(combined.samplerLocation.binding);

                GLuint samplerIndex = indices[combined.samplerLocation.group][bindingIndex];
                mUnitsForSamplers[samplerIndex].push_back({textureUnit, shouldUseFiltering});
            }
        }

        textureUnit++;
    }

    for (GLuint glShader : glShaders) {
        gl.DetachShader(mProgram, glShader);
        gl.DeleteShader(glShader);
    }

    mInternalUniformBufferBinding = layout->GetInternalUniformBinding();

    return {};
}

void PipelineGL::DeleteProgram(const OpenGLFunctions& gl) {
    gl.DeleteProgram(mProgram);
}

const std::vector<PipelineGL::SamplerUnit>& PipelineGL::GetTextureUnitsForSampler(
    GLuint index) const {
    ASSERT(index < mUnitsForSamplers.size());
    return mUnitsForSamplers[index];
}

const std::vector<GLuint>& PipelineGL::GetTextureUnitsForTextureView(GLuint index) const {
    ASSERT(index < mUnitsForTextures.size());
    return mUnitsForTextures[index];
}

GLuint PipelineGL::GetProgramHandle() const {
    return mProgram;
}

void PipelineGL::ApplyNow(const OpenGLFunctions& gl) {
    gl.UseProgram(mProgram);
    for (GLuint unit : mPlaceholderSamplerUnits) {
        ASSERT(mPlaceholderSampler.Get() != nullptr);
        gl.BindSampler(unit, mPlaceholderSampler->GetNonFilteringHandle());
    }

    if (mTextureBuiltinsBuffer.Get() != nullptr) {
        // printf("\n!!!!!! %d\n", mNeedsTextureBuiltinUniformBuffer);
        // gl.BindBuffer(kMaxBindGroups + 1, mTextureBuiltinsBuffer->GetHandle());
        // gl.BindBufferBase(GL_UNIFORM_BUFFER, 5, mTextureBuiltinsBuffer->GetHandle());   // test
        gl.BindBufferBase(GL_UNIFORM_BUFFER, mInternalUniformBufferBinding,
                          mTextureBuiltinsBuffer->GetHandle());  // test
    }
}

void PipelineGL::UpdateTextureBuiltinsUniformData(const OpenGLFunctions& gl,
                                                  const TextureView* view,
                                                  BindGroupIndex groupIndex,
                                                  BindingIndex bindingIndex) {
    if (mTextureBuiltinsBuffer.Get() == nullptr || mBindingPointBuiltinsDataInfo.empty()) {
        return;
    }

    // ?? CombineSamplers transform caused texture binding mismatch?
    printf("\n\n-------------------\n");
    for (const auto& e : mBindingPointBuiltinsDataInfo) {
        printf(" (%u, %u)  ->  type: %u, offset: %u\n", e.first.group, e.first.binding,
               e.second.first, e.second.second);
    }
    printf("-------------------\n\n");

    // need combineSampler string -> find corresponding textureView binding
    auto iter = mBindingPointBuiltinsDataInfo.find(
        tint::BindingPoint{static_cast<uint32_t>(groupIndex), static_cast<uint32_t>(bindingIndex)});
    printf("\n finding (%u, %u) \n\n\n", static_cast<uint32_t>(groupIndex),
           static_cast<uint32_t>(bindingIndex));
    if (iter == mBindingPointBuiltinsDataInfo.end()) {
        return;
    }

    // update data (from texture bindgroup)
    const tint::TextureBuiltinsFromUniformOptions::DataType dataType = iter->second.first;
    const uint32_t byteOffset = iter->second.second;

    uint32_t data;
    switch (dataType) {
        case tint::TextureBuiltinsFromUniformOptions::DataType::TextureNumLevels:
            data = view->GetLevelCount();
            break;
        case tint::TextureBuiltinsFromUniformOptions::DataType::TextureNumSamples:
            data = view->GetTexture()->GetSampleCount();
            break;
    }
    gl.BindBuffer(GL_UNIFORM_BUFFER, mTextureBuiltinsBuffer->GetHandle());
    gl.BufferSubData(GL_UNIFORM_BUFFER, byteOffset, sizeof(uint32_t), &data);
    gl.BindBuffer(GL_UNIFORM_BUFFER, 0);
}

}  // namespace dawn::native::opengl
