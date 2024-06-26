// Copyright 2017 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * This is a short test which creates an instance, requests an adapter and device
 * and links with the monolithic shared library webgpu_dawn.
 * It does not draw anything.
 */

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "dawn/webgpu_cpp_print.h"
#include "webgpu/webgpu_cpp.h"

void PrintDeviceError(WGPUErrorType errorType, const char* message, void*) {
    const char* errorTypeName = "";
    switch (errorType) {
        case WGPUErrorType_Validation:
            errorTypeName = "Validation";
            break;
        case WGPUErrorType_OutOfMemory:
            errorTypeName = "Out of memory";
            break;
        case WGPUErrorType_Unknown:
            errorTypeName = "Unknown";
            break;
        case WGPUErrorType_DeviceLost:
            errorTypeName = "Device lost";
            break;
        default:
            break;
    }
    std::cerr << errorTypeName << " error: " << message << '\n';
}

void PrintDeviceLoss(const WGPUDevice* device,
                     WGPUDeviceLostReason reason,
                     const char* message,
                     void* userdata) {
    const char* reasonName = "";
    switch (reason) {
        case WGPUDeviceLostReason_Unknown:
            reasonName = "Unknown";
            break;
        case WGPUDeviceLostReason_Destroyed:
            reasonName = "Destroyed";
            break;
        case WGPUDeviceLostReason_InstanceDropped:
            reasonName = "InstanceDropped";
            break;
        case WGPUDeviceLostReason_FailedCreation:
            reasonName = "FailedCreation";
            break;
        default:
            break;
    }
    std::cerr << "Device lost because of " << reasonName << ": message\n";
}

std::string AsHex(uint32_t val) {
    std::stringstream hex;
    hex << "0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << val;
    return hex.str();
}

std::string AdapterPropertiesToString(const wgpu::AdapterProperties& props) {
    std::stringstream out;
    out << "VendorID: " << AsHex(props.vendorID) << "\n";
    out << "Vendor: " << props.vendorName << "\n";
    out << "Architecture: " << props.architecture << "\n";
    out << "DeviceID: " << AsHex(props.deviceID) << "\n";
    out << "Name: " << props.name << "\n";
    out << "Driver description: " << props.driverDescription << "\n";
    out << "Adapter Type: " << props.adapterType << "\n";
    out << "Backend Type: " << props.backendType << "\n";

    return out.str();
}

std::string PowerPreferenceToString(const wgpu::DawnAdapterPropertiesPowerPreference& prop) {
    switch (prop.powerPreference) {
        case wgpu::PowerPreference::LowPower:
            return "low power";
        case wgpu::PowerPreference::HighPerformance:
            return "high performance";
        case wgpu::PowerPreference::Undefined:
            return "<undefined>";
    }
    return "<unknown>";
}

void DumpAdapterProperties(const wgpu::Adapter& adapter) {
    wgpu::DawnAdapterPropertiesPowerPreference power_props{};

    wgpu::AdapterProperties properties{};
    properties.nextInChain = &power_props;

    adapter.GetProperties(&properties);
    std::cout << AdapterPropertiesToString(properties);
    std::cout << "Power: " << PowerPreferenceToString(power_props) << "\n";
    std::cout << "\n";
}

void DumpAdapter(const wgpu::Adapter& adapter) {
    std::cout << "Adapter\n";
    std::cout << "=======\n";

    DumpAdapterProperties(adapter);
}

int main() {
    wgpu::InstanceDescriptor instanceDescriptor{};
    instanceDescriptor.features.timedWaitAnyEnable = true;
    wgpu::Instance instance = wgpu::CreateInstance(&instanceDescriptor);
    if (instance == nullptr) {
        std::cerr << "Instance creation failed!\n";
        return EXIT_FAILURE;
    }
    // Synchronously request the adapter.
    wgpu::RequestAdapterOptions options = {};
    wgpu::Adapter adapter;
    instance.WaitAny(instance.RequestAdapter(
                         &options, {nullptr, wgpu::CallbackMode::WaitAnyOnly,
                                    [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                                       const char* message, void* userdata) {
                                        if (status != WGPURequestAdapterStatus_Success) {
                                            std::cerr << "Failed to get an adapter:" << message;
                                            return;
                                        }
                                        *static_cast<wgpu::Adapter*>(userdata) =
                                            wgpu::Adapter::Acquire(adapter);
                                    },
                                    &adapter}),
                     UINT64_MAX);
    if (adapter == nullptr) {
        std::cerr << "RequestAdapter failed!\n";
        return EXIT_FAILURE;
    }
    DumpAdapter(adapter);

    // Synchronously request the device.
    wgpu::DeviceDescriptor deviceDesc;
    deviceDesc.uncapturedErrorCallbackInfo = {nullptr, PrintDeviceError, nullptr};
    deviceDesc.deviceLostCallbackInfo = {nullptr, wgpu::CallbackMode::AllowSpontaneous,
                                         PrintDeviceLoss, nullptr};
    wgpu::Device device;
    instance.WaitAny(adapter.RequestDevice(
                         &deviceDesc, {nullptr, wgpu::CallbackMode::WaitAnyOnly,
                                       [](WGPURequestDeviceStatus status, WGPUDevice device,
                                          const char* message, void* userdata) {
                                           if (status != WGPURequestDeviceStatus_Success) {
                                               std::cerr << "Failed to get an device:" << message;
                                               return;
                                           }
                                           *static_cast<wgpu::Device*>(userdata) =
                                               wgpu::Device::Acquire(device);
                                       },
                                       &device}),
                     UINT64_MAX);

    if (device == nullptr) {
        std::cerr << "RequestDevice failed!\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
