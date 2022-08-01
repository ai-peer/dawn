// Copyright 2022 The Dawn Authors
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

#include "dawn/native/ApplyClearValueWithDrawHelper.h"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "dawn/native/BindGroup.h"
#include "dawn/native/BindGroupLayout.h"
#include "dawn/native/Device.h"
#include "dawn/native/InternalPipelineStore.h"
#include "dawn/native/ObjectContentHasher.h"
#include "dawn/native/RenderPassEncoder.h"
#include "dawn/native/RenderPipeline.h"
#include "dawn/native/utils/WGPUHelpers.h"

namespace dawn::native {

namespace {

static const char kVSSource[] = R"(
@vertex
fn main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
    var pos = array<vec2<f32>, 6>(
        vec2<f32>( 0.0, -1.0),
        vec2<f32>( 1.0, -1.0),
        vec2<f32>( 0.0,  1.0),
        vec2<f32>( 0.0,  1.0),
        vec2<f32>( 1.0, -1.0),
        vec2<f32>( 1.0,  1.0));

        return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
})";

const char* GetTextureComponentTypeString(wgpu::TextureComponentType type) {
    switch (type) {
        case wgpu::TextureComponentType::Uint:
            return "u32";
        case wgpu::TextureComponentType::Sint:
            return "i32";
        case wgpu::TextureComponentType::Float:
            return "f32";
        case wgpu::TextureComponentType::DepthComparison:
        default:
            UNREACHABLE();
            return "";
    }
}

std::string ConstructFragmentShader(const RenderPassDescriptor* renderPassDescriptor) {
    std::array<wgpu::TextureComponentType, kMaxColorAttachments> componentTypes;
    componentTypes.fill(wgpu::TextureComponentType::Uint);
    for (uint32_t i = 0; i < renderPassDescriptor->colorAttachmentCount; ++i) {
        if (renderPassDescriptor->colorAttachments[i].view != nullptr) {
            componentTypes[i] = renderPassDescriptor->colorAttachments[i]
                                    .view->GetFormat()
                                    .GetAspectInfo(Aspect::Color)
                                    .baseType;
        }
    }

    std::ostringstream stream;

    // To simplify the implementation we always declare kMaxColorAttachments fragment outputs. It's
    // OK because the fragment output will always be discarded when there is no corresponding color
    // attachment.
    stream << "struct OutputColor {" << std::endl;
    for (uint32_t i = 0; i < kMaxColorAttachments; ++i) {
        const char* type = GetTextureComponentTypeString(componentTypes[i]);
        stream << "@location(" << i << ") output" << i << " : vec4<" << type << ">," << std::endl;
    }
    stream << "}" << std::endl;

    stream << "struct ClearColors {" << std::endl;
    for (uint32_t i = 0; i < kMaxColorAttachments; ++i) {
        const char* type = GetTextureComponentTypeString(componentTypes[i]);
        stream << "color" << i << " : vec4<" << type << ">," << std::endl;
    }
    stream << "}" << std::endl;

    stream << R"(
@group(0) @binding(0) var<uniform> clearColors : ClearColors;

@fragment
fn main() -> OutputColor {
    var outputColor : OutputColor;
)";

    for (uint32_t i = 0; i < kMaxColorAttachments; ++i) {
        stream << "outputColor.output" << i << " = clearColors.color" << i << ";" << std::endl;
    }

    stream << R"(
return outputColor;
})";

    return stream.str();
}

KeyOfApplyClearColorWithDrawPipelines GetKeyOfApplyClearColorWithDrawPipelines(
    const RenderPassDescriptor* renderPassDescriptor) {
    KeyOfApplyClearColorWithDrawPipelines key;
    for (uint32_t i = 0; i < renderPassDescriptor->colorAttachmentCount; ++i) {
        if (renderPassDescriptor->colorAttachments[i].view == nullptr) {
            key[i] = wgpu::TextureFormat::Undefined;
        } else {
            key[i] = renderPassDescriptor->colorAttachments[i].view->GetFormat().format;
        }
    }
    return key;
}

RenderPipelineBase* GetCachedPipeline(InternalPipelineStore* store,
                                      KeyOfApplyClearColorWithDrawPipelines key) {
    auto iter = store->applyClearColorWithDrawPipelines.find(key);
    if (iter != store->applyClearColorWithDrawPipelines.end()) {
        return iter->second.Get();
    }
    return nullptr;
}

ResultOrError<RenderPipelineBase*> GetOrCreateApplyClearValueWithDrawPipeline(
    DeviceBase* device,
    InternalPipelineStore* store,
    const RenderPassDescriptor* renderPassDescriptor) {
    KeyOfApplyClearColorWithDrawPipelines key =
        GetKeyOfApplyClearColorWithDrawPipelines(renderPassDescriptor);

    RenderPipelineBase* cachedPipeline = GetCachedPipeline(store, key);
    if (cachedPipeline != nullptr) {
        return cachedPipeline;
    }

    Ref<ShaderModuleBase> vertexModule;
    DAWN_TRY_ASSIGN(vertexModule, utils::CreateShaderModule(device, kVSSource));

    // Prepare vertex stage
    VertexState vertex = {};
    vertex.module = vertexModule.Get();
    vertex.entryPoint = "main";

    std::string fragmentShader = ConstructFragmentShader(renderPassDescriptor);
    Ref<ShaderModuleBase> fragmentModule;
    DAWN_TRY_ASSIGN(fragmentModule, utils::CreateShaderModule(device, fragmentShader.c_str()));

    // Prepare fragment stage
    FragmentState fragment = {};
    fragment.module = fragmentModule.Get();
    fragment.entryPoint = "main";

    // Prepare color state.
    std::vector<ColorTargetState> targets(kMaxColorAttachments);
    for (uint32_t i = 0; i < renderPassDescriptor->colorAttachmentCount; ++i) {
        if (renderPassDescriptor->colorAttachments[i].view == nullptr) {
            targets[i].writeMask = wgpu::ColorWriteMask::None;
        } else {
            targets[i].format = renderPassDescriptor->colorAttachments[i].view->GetFormat().format;
        }
    }

    // Create RenderPipeline
    RenderPipelineDescriptor renderPipelineDesc = {};

    renderPipelineDesc.vertex = vertex;
    renderPipelineDesc.fragment = &fragment;
    renderPipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    fragment.targetCount = renderPassDescriptor->colorAttachmentCount;
    fragment.targets = targets.data();

    Ref<RenderPipelineBase> pipeline;
    DAWN_TRY_ASSIGN(pipeline, device->CreateRenderPipeline(&renderPipelineDesc));
    store->applyClearColorWithDrawPipelines.insert({key, std::move(pipeline)});

    return GetCachedPipeline(store, key);
}

Color GetClearValue(const RenderPassColorAttachment& attachment) {
    return HasDeprecatedColor(attachment) ? attachment.clearColor : attachment.clearValue;
}

bool ShouldApplyClearValueWithDraw(const RenderPassColorAttachment& colorAttachmentInfo) {
    const Format& format = colorAttachmentInfo.view->GetFormat();

    // Currently we only need to add this workaround on i32 and u32 formats on D3D12.
    switch (format.format) {
        case wgpu::TextureFormat::R32Sint:
        case wgpu::TextureFormat::RG32Sint:
        case wgpu::TextureFormat::RGBA32Sint:
        case wgpu::TextureFormat::R32Uint:
        case wgpu::TextureFormat::RG32Uint:
        case wgpu::TextureFormat::RGBA32Uint:
            break;
        default:
            return false;
    }

    Color clearValue = GetClearValue(colorAttachmentInfo);
    switch (format.GetAspectInfo(Aspect::Color).baseType) {
        case wgpu::TextureComponentType::Uint: {
            constexpr double kMaxUintRepresentableInFloat = 1 << std::numeric_limits<float>::digits;
            if (clearValue.r > kMaxUintRepresentableInFloat ||
                clearValue.g > kMaxUintRepresentableInFloat ||
                clearValue.b > kMaxUintRepresentableInFloat ||
                clearValue.a > kMaxUintRepresentableInFloat) {
                return true;
            }
            break;
        }
        case wgpu::TextureComponentType::Sint: {
            constexpr double kMaxSintRepresentableInFloat = 1 << std::numeric_limits<float>::digits;
            constexpr double kMinSintRepresentableInFloat = -kMaxSintRepresentableInFloat;
            if (clearValue.r > kMaxSintRepresentableInFloat ||
                clearValue.r < kMinSintRepresentableInFloat ||
                clearValue.g > kMaxSintRepresentableInFloat ||
                clearValue.g < kMinSintRepresentableInFloat ||
                clearValue.b > kMaxSintRepresentableInFloat ||
                clearValue.b < kMinSintRepresentableInFloat ||
                clearValue.a > kMaxSintRepresentableInFloat ||
                clearValue.a < kMinSintRepresentableInFloat) {
                return true;
            }
            break;
        }
        case wgpu::TextureComponentType::Float:
        case wgpu::TextureComponentType::DepthComparison:
        default:
            UNREACHABLE();
            break;
    }

    return false;
}

}  // anonymous namespace

size_t KeyOfApplyClearColorWithDrawPipelinesHashFunc::operator()(
    KeyOfApplyClearColorWithDrawPipelines key) const {
    size_t hash = 0;

    for (wgpu::TextureFormat format : key) {
        HashCombine(&hash, format);
    }

    return hash;
}

bool KeyOfApplyClearColorWithDrawPipelinesEqualityFunc::operator()(
    KeyOfApplyClearColorWithDrawPipelines key1,
    const KeyOfApplyClearColorWithDrawPipelines key2) const {
    for (uint32_t i = 0; i < kMaxColorAttachments; ++i) {
        if (key1[i] != key2[i]) {
            return false;
        }
    }
    return true;
}

bool ShouldApplyClearValueWithDraw(DeviceBase* device,
                                   const RenderPassDescriptor* renderPassDescriptor) {
    if (device->IsToggleEnabled(Toggle::D3D12ApplyLargeIntegerAsClearValueWithDraw)) {
        for (uint32_t i = 0; i < renderPassDescriptor->colorAttachmentCount; ++i) {
            if (renderPassDescriptor->colorAttachments[i].view == nullptr) {
                continue;
            }
            if (ShouldApplyClearValueWithDraw(renderPassDescriptor->colorAttachments[i])) {
                return true;
            }
        }
    }
    return false;
}

ResultOrError<Ref<BufferBase>> CreateUniformBufferWithClearValues(
    DeviceBase* device,
    const RenderPassDescriptor* renderPassDescriptor) {
    std::vector<uint8_t> clearValues(sizeof(uint32_t) * 4 * kMaxColorAttachments);
    for (uint32_t i = 0; i < renderPassDescriptor->colorAttachmentCount; ++i) {
        if (renderPassDescriptor->colorAttachments[i].view == nullptr) {
            continue;
        }

        const Format& format = renderPassDescriptor->colorAttachments[i].view->GetFormat();
        wgpu::TextureComponentType baseType = format.GetAspectInfo(Aspect::Color).baseType;

        Color initialClearValue = GetClearValue(renderPassDescriptor->colorAttachments[i]);
        Color clearValue = ClampClearColorValueToLegalRange(initialClearValue, format);
        uint32_t offset = i * (sizeof(uint32_t) * 4);
        switch (baseType) {
            case wgpu::TextureComponentType::Uint: {
                uint32_t* clearValuePtr = reinterpret_cast<uint32_t*>(clearValues.data() + offset);
                clearValuePtr[0] = static_cast<uint32_t>(clearValue.r);
                clearValuePtr[1] = static_cast<uint32_t>(clearValue.g);
                clearValuePtr[2] = static_cast<uint32_t>(clearValue.b);
                clearValuePtr[3] = static_cast<uint32_t>(clearValue.a);
                break;
            }
            case wgpu::TextureComponentType::Sint: {
                int32_t* clearValuePtr = reinterpret_cast<int32_t*>(clearValues.data() + offset);
                clearValuePtr[0] = static_cast<int32_t>(clearValue.r);
                clearValuePtr[1] = static_cast<int32_t>(clearValue.g);
                clearValuePtr[2] = static_cast<int32_t>(clearValue.b);
                clearValuePtr[3] = static_cast<int32_t>(clearValue.a);
                break;
            }
            case wgpu::TextureComponentType::Float: {
                float* clearValuePtr = reinterpret_cast<float*>(clearValues.data() + offset);
                clearValuePtr[0] = static_cast<float>(clearValue.r);
                clearValuePtr[1] = static_cast<float>(clearValue.g);
                clearValuePtr[2] = static_cast<float>(clearValue.b);
                clearValuePtr[3] = static_cast<float>(clearValue.a);
                break;
            }

            case wgpu::TextureComponentType::DepthComparison:
            default:
                UNREACHABLE();
                break;
        }
    }

    Ref<BufferBase> outputBuffer;
    DAWN_TRY_ASSIGN(
        outputBuffer,
        utils::CreateBufferFromData(device, wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform,
                                    clearValues.data(), clearValues.size()));

    return std::move(outputBuffer);
}

MaybeError ApplyClearValueWithDraw(RenderPassEncoder* renderPassEncoder,
                                   Ref<BufferBase> uniformBufferWithClearData,
                                   const RenderPassDescriptor* renderPassDescriptor) {
    DeviceBase* device = renderPassEncoder->GetDevice();

    RenderPipelineBase* pipeline = nullptr;
    DAWN_TRY_ASSIGN(pipeline,
                    GetOrCreateApplyClearValueWithDrawPipeline(
                        device, device->GetInternalPipelineStore(), renderPassDescriptor));

    Ref<BindGroupLayoutBase> layout;
    DAWN_TRY_ASSIGN(layout, pipeline->GetBindGroupLayout(0));

    Ref<BindGroupBase> bindGroup;
    DAWN_TRY_ASSIGN(bindGroup,
                    utils::MakeBindGroup(device, layout, {{0, uniformBufferWithClearData}},
                                         UsageValidationMode::Internal));

    renderPassEncoder->APISetBindGroup(0, bindGroup.Get());
    renderPassEncoder->APISetPipeline(pipeline);
    renderPassEncoder->APIDraw(6);

    return {};
}

}  // namespace dawn::native
