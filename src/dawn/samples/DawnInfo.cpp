// Copyright 2023 The Dawn Authors
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

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "dawn/native/DawnNative.h"
#include "dawn/native/Toggles.h"
#include "dawn/webgpu_cpp.h"

namespace {

void FindInChain(WGPUChainedStructOut* chain, WGPUDawnAdapterPropertiesPowerPreference** out) {
    for (; chain; chain = chain->next) {
        if (chain->sType == WGPUSType_DawnAdapterPropertiesPowerPreference) {
            *out = reinterpret_cast<WGPUDawnAdapterPropertiesPowerPreference*>(chain);
            break;
        }
    }
}

std::string wrap_string(const std::string& in) {
    std::stringstream out;

    size_t last_space = 0;
    size_t start_pos = 0;
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == ' ') {
            last_space = i;
        } else if (in[i] == '\n') {
            last_space = i;
        }

        if ((i - start_pos) != 0 && ((i - start_pos) % 75) == 0) {
            out << "    " << in.substr(start_pos, last_space - start_pos) << "\n";
            start_pos = last_space + 1;
            last_space = start_pos;
        }
    }
    out << "    " << in.substr(start_pos, in.size() - start_pos);

    return out.str();
}

std::string AdapterTypeToString(WGPUAdapterType type) {
    switch (type) {
        case WGPUAdapterType_DiscreteGPU:
            return "discrete GPU";
        case WGPUAdapterType_IntegratedGPU:
            return "integrated GPU";
        case WGPUAdapterType_CPU:
            return "CPU";
        case WGPUAdapterType_Force32:
        case WGPUAdapterType_Unknown:
            break;
    }
    return "unknown";
}

std::string BackendTypeToString(WGPUBackendType type) {
    switch (type) {
        case WGPUBackendType_Null:
            return "Null";
        case WGPUBackendType_WebGPU:
            return "WebGPU";
        case WGPUBackendType_D3D11:
            return "D3D11";
        case WGPUBackendType_D3D12:
            return "D3D12";
        case WGPUBackendType_Metal:
            return "Metal";
        case WGPUBackendType_Vulkan:
            return "Vulkan";
        case WGPUBackendType_OpenGL:
            return "OpenGL";
        case WGPUBackendType_OpenGLES:
            return "OpenGLES";
        case WGPUBackendType_Force32:
        case WGPUBackendType_Undefined:
            return "Undefined";
    }
    return "unknown";
}

std::string PowerPreferenceToString(const WGPUDawnAdapterPropertiesPowerPreference& prop) {
    switch (prop.powerPreference) {
        case WGPUPowerPreference_LowPower:
            return "low power";
        case WGPUPowerPreference_HighPerformance:
            return "high performance";
        case WGPUPowerPreference_Undefined:
            return "<undefined>";
        case WGPUPowerPreference_Force32:
            break;
    }
    return "<unknown>";
}

std::string AdapterPropertiesToString(const WGPUAdapterProperties& props) {
    std::stringstream out;
    out << "VendorID: " << props.vendorID << "\n";
    out << "Vendor: " << props.vendorName << "\n";
    out << "Architecture: " << props.architecture << "\n";
    out << "DeviceID: " << props.deviceID << "\n";
    out << "Name: " << props.name << "\n";
    out << "Driver description: " << props.driverDescription << "\n";
    out << "Adapter Type: " << AdapterTypeToString(props.adapterType) << "\n";
    out << "Backend Type: " << BackendTypeToString(props.backendType) << "\n";

    WGPUDawnAdapterPropertiesPowerPreference* powerPrefs = nullptr;
    FindInChain(props.nextInChain, &powerPrefs);
    if (powerPrefs) {
        out << "Power: " << PowerPreferenceToString(*powerPrefs);
    }

    return out.str();
}

std::string FormatNumber(uint64_t num) {
    auto s = std::to_string(num);
    std::stringstream ret;

    auto remainder = s.length() % 3;
    ret << s.substr(0, remainder);
    for (size_t i = remainder; i < s.length(); i += 3) {
        if (i > 0) {
            ret << ",";
        }
        ret << s.substr(i, 3);
    }
    return ret.str();
}

std::string LimitsToString(const WGPULimits& limits, const std::string& indent) {
    std::stringstream out;

    out << indent << "maxTextureDimension1D: " << FormatNumber(limits.maxTextureDimension1D)
        << "\n";
    out << indent << "maxTextureDimension2D: " << FormatNumber(limits.maxTextureDimension2D)
        << "\n";
    out << indent << "maxTextureDimension3D: " << FormatNumber(limits.maxTextureDimension3D)
        << "\n";
    out << indent << "maxTextureArrayLayers: " << FormatNumber(limits.maxTextureArrayLayers)
        << "\n";
    out << indent << "maxBindGroups: " << FormatNumber(limits.maxBindGroups) << "\n";
    out << indent << "maxDynamicUniformBuffersPerPipelineLayout: "
        << FormatNumber(limits.maxDynamicUniformBuffersPerPipelineLayout) << "\n";
    out << indent << "maxDynamicStorageBuffersPerPipelineLayout: "
        << FormatNumber(limits.maxDynamicStorageBuffersPerPipelineLayout) << "\n";
    out << indent << "maxSampledTexturesPerShaderStage: "
        << FormatNumber(limits.maxSampledTexturesPerShaderStage) << "\n";
    out << indent << "maxSamplersPerShaderStage: " << FormatNumber(limits.maxSamplersPerShaderStage)
        << "\n";
    out << indent << "maxStorageBuffersPerShaderStage: "
        << FormatNumber(limits.maxStorageBuffersPerShaderStage) << "\n";
    out << indent << "maxStorageTexturesPerShaderStage: "
        << FormatNumber(limits.maxStorageTexturesPerShaderStage) << "\n";
    out << indent << "maxUniformBuffersPerShaderStage: "
        << FormatNumber(limits.maxUniformBuffersPerShaderStage) << "\n";
    out << indent
        << "maxUniformBufferBindingSize: " << FormatNumber(limits.maxUniformBufferBindingSize)
        << "\n";
    out << indent
        << "maxStorageBufferBindingSize: " << FormatNumber(limits.maxStorageBufferBindingSize)
        << "\n";
    out << indent << "minUniformBufferOffsetAlignment: "
        << FormatNumber(limits.minUniformBufferOffsetAlignment) << "\n";
    out << indent << "minStorageBufferOffsetAlignment: "
        << FormatNumber(limits.minStorageBufferOffsetAlignment) << "\n";
    out << indent << "maxVertexBuffers: " << FormatNumber(limits.maxVertexBuffers) << "\n";
    out << indent << "maxVertexAttributes: " << FormatNumber(limits.maxVertexAttributes) << "\n";
    out << indent
        << "maxVertexBufferArrayStride: " << FormatNumber(limits.maxVertexBufferArrayStride)
        << "\n";
    out << indent
        << "maxInterStageShaderComponents: " << FormatNumber(limits.maxInterStageShaderComponents)
        << "\n";
    out << indent
        << "maxInterStageShaderVariables: " << FormatNumber(limits.maxInterStageShaderVariables)
        << "\n";
    out << indent << "maxColorAttachments: " << FormatNumber(limits.maxColorAttachments) << "\n";
    out << indent
        << "maxComputeWorkgroupStorageSize: " << FormatNumber(limits.maxComputeWorkgroupStorageSize)
        << "\n";
    out << indent << "maxComputeInvocationsPerWorkgroup: "
        << FormatNumber(limits.maxComputeInvocationsPerWorkgroup) << "\n";
    out << indent << "maxComputeWorkgroupSizeX: " << FormatNumber(limits.maxComputeWorkgroupSizeX)
        << "\n";
    out << indent << "maxComputeWorkgroupSizeY: " << FormatNumber(limits.maxComputeWorkgroupSizeY)
        << "\n";
    out << indent << "maxComputeWorkgroupSizeZ: " << FormatNumber(limits.maxComputeWorkgroupSizeZ)
        << "\n";
    out << indent << "maxComputeWorkgroupsPerDimension: "
        << FormatNumber(limits.maxComputeWorkgroupsPerDimension) << "\n";

    return out.str();
}

void DumpAdapterProperties(const dawn::native::Adapter& adapter) {
    WGPUDawnAdapterPropertiesPowerPreference power_props{};
    power_props.chain.sType = WGPUSType_DawnAdapterPropertiesPowerPreference;

    WGPUAdapterProperties properties{};
    properties.nextInChain = &power_props.chain;

    adapter.GetProperties(&properties);
    std::cout << AdapterPropertiesToString(properties) << "\n";

    // This should call the free method, but it's crashing?
    // wgpuAdapterPropertiesFreeMembers(properties);
    delete[] properties.vendorName;
}

// void DumpAdapterExtensions(const dawn::native::Adapter& adapter) {
//     std::vector<const char*> extensions = adapter.GetSupportedExtensions();
//     std::cout << "  Extensions\n";
//     std::cout << "  ==========\n";
//     for (const auto& ext : extensions) {
//         std::cout << "    " << ext << "\n";
//     }
// }

void DumpAdapterFeatures(const dawn::native::Adapter& adapter) {
    std::vector<const char*> features = adapter.GetSupportedFeatures();
    std::cout << "  Features\n";
    std::cout << "  ========\n";
    for (const auto& f : features) {
        std::cout << "    " << f << "\n";
    }
}

void DumpAdapterLimits(const dawn::native::Adapter& adapter) {
    WGPUSupportedLimits adapterLimits;
    adapter.GetLimits(&adapterLimits);
    {
        std::cout << "\n";
        std::cout << "  Adapter Limits\n";
        std::cout << "  ==============\n";
        std::cout << LimitsToString(adapterLimits.limits, "    ") << "\n";
    }
}

void DumpAdapter(const dawn::native::Adapter& adapter) {
    std::cout << "Adapter\n";
    std::cout << "=======\n";

    DumpAdapterProperties(adapter);
    DumpAdapterFeatures(adapter);
    //    DumpAdapterExtensions(adapter);
    DumpAdapterLimits(adapter);
}

}  // namespace

int main(int argc, const char* argv[]) {
    auto toggles = dawn::native::TogglesInfo::AllToggleInfos();

    std::cout << "Toggles\n";
    std::cout << "=======\n";
    bool first = true;
    for (const auto* info : toggles) {
        if (!first) {
            std::cout << "\n";
        }
        first = false;
        std::cout << "  Name: " << info->name << "\n";
        std::cout << wrap_string(info->description) << "\n";
        std::cout << "    " << info->url << "\n";
    }
    std::cout << "\n";

    auto instance = std::make_unique<dawn::native::Instance>();
    std::vector<dawn::native::Adapter> adapters = instance->EnumerateAdapters();

    for (const auto& adapter : adapters) {
        DumpAdapter(adapter);
    }
}
