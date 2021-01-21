// Copyright 2021 The Dawn Authors
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

#include "utils/InstanceHolder.h"

#include "common/Assert.h"
#include "common/Log.h"
#include "common/SystemUtils.h"
#include "dawn/dawn_proc.h"
#include "dawn_wire/WireClient.h"
#include "dawn_wire/WireServer.h"
#include "utils/GLFWUtils.h"
#include "utils/TerribleCommandBuffer.h"

#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

#if defined(DAWN_ENABLE_BACKEND_OPENGL)
#    include "GLFW/glfw3.h"
#    include "dawn_native/OpenGLBackend.h"
#endif  // DAWN_ENABLE_BACKEND_OPENGL

namespace utils {

    namespace {
        const char* BackendName(wgpu::BackendType type) {
            switch (type) {
                case wgpu::BackendType::D3D12:
                    return "D3D12";
                case wgpu::BackendType::Metal:
                    return "Metal";
                case wgpu::BackendType::Null:
                    return "Null";
                case wgpu::BackendType::OpenGL:
                    return "OpenGL";
                case wgpu::BackendType::OpenGLES:
                    return "OpenGLES";
                case wgpu::BackendType::Vulkan:
                    return "Vulkan";
                default:
                    UNREACHABLE();
            }
        }

        const char* AdapterTypeName(wgpu::AdapterType type) {
            switch (type) {
                case wgpu::AdapterType::DiscreteGPU:
                    return "Discrete GPU";
                case wgpu::AdapterType::IntegratedGPU:
                    return "Integrated GPU";
                case wgpu::AdapterType::CPU:
                    return "CPU";
                case wgpu::AdapterType::Unknown:
                    return "Unknown";
                default:
                    UNREACHABLE();
            }
        }

        class WireServerTraceLayer : public dawn_wire::CommandHandler {
            struct ScopedWireTraceImpl : InstanceHolder::ScopedWireTrace {
                ScopedWireTraceImpl(std::ofstream* file) : file(file) {
                }
                ~ScopedWireTraceImpl() override {
                    file->close();
                }

                std::ofstream* file;
            };

          public:
            WireServerTraceLayer(std::string dir, dawn_wire::CommandHandler* handler)
                : dawn_wire::CommandHandler(), mDir(dir), mHandler(handler) {
            }

            std::unique_ptr<InstanceHolder::ScopedWireTrace> BeginScopedWireTrace(
                std::string name) {
                std::string filename = mDir + name;
                // Replace slashes in gtest names with underscores so everything is in one
                // directory.
                std::replace(filename.begin(), filename.end(), '/', '_');

                ASSERT(!mFile.is_open());
                mFile.open(filename,
                           std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
                return std::unique_ptr<InstanceHolder::ScopedWireTrace>(
                    new ScopedWireTraceImpl(&mFile));
            }

            const volatile char* HandleCommands(const volatile char* commands,
                                                size_t size) override {
                ASSERT(mFile.is_open());
                mFile.write(const_cast<const char*>(commands), size);
                return mHandler->HandleCommands(commands, size);
            }

          private:
            std::string mDir;
            dawn_wire::CommandHandler* mHandler;
            std::ofstream mFile;
        };

    }  // anonymous namespace

    // static
    InstanceHolder::Options InstanceHolder::Options::FromCommandLine(int argc, char** argv) {
        Options options = {};

        size_t argLen = 0;  // Set when parsing --arg=X arguments
        for (int i = 1; i < argc; ++i) {
            if (strcmp("-w", argv[i]) == 0 || strcmp("--use-wire", argv[i]) == 0) {
                options.useWire = true;
                continue;
            }

            if (strcmp("-d", argv[i]) == 0 || strcmp("--enable-backend-validation", argv[i]) == 0) {
                options.enableBackendValidation = true;
                continue;
            }

            if (strcmp("-c", argv[i]) == 0 || strcmp("--begin-capture-on-startup", argv[i]) == 0) {
                options.beginCaptureOnStartup = true;
                continue;
            }

            constexpr const char kEnableTogglesSwitch[] = "--enable-toggles=";
            argLen = sizeof(kEnableTogglesSwitch) - 1;
            if (strncmp(argv[i], kEnableTogglesSwitch, argLen) == 0) {
                std::string toggle;
                std::stringstream toggles(argv[i] + argLen);
                while (getline(toggles, toggle, ',')) {
                    options.enabledToggles.push_back(toggle);
                }
                continue;
            }

            constexpr const char kDisableTogglesSwitch[] = "--disable-toggles=";
            argLen = sizeof(kDisableTogglesSwitch) - 1;
            if (strncmp(argv[i], kDisableTogglesSwitch, argLen) == 0) {
                std::string toggle;
                std::stringstream toggles(argv[i] + argLen);
                while (getline(toggles, toggle, ',')) {
                    options.disabledToggles.push_back(toggle);
                }
                continue;
            }

            constexpr const char kVendorIdFilterArg[] = "--adapter-vendor-id=";
            argLen = sizeof(kVendorIdFilterArg) - 1;
            if (strncmp(argv[i], kVendorIdFilterArg, argLen) == 0) {
                const char* vendorIdFilter = argv[i] + argLen;
                if (vendorIdFilter[0] != '\0') {
                    options.defaultVendorIdFilter = strtoul(vendorIdFilter, nullptr, 16);
                    // Set filter flag if vendor id is non-zero.
                    options.hasDefaultVendorIdFilter = options.defaultVendorIdFilter != 0;
                }
                continue;
            }

            constexpr const char kExclusiveDeviceTypePreferenceArg[] =
                "--exclusive-device-type-preference=";
            argLen = sizeof(kExclusiveDeviceTypePreferenceArg) - 1;
            if (strncmp(argv[i], kExclusiveDeviceTypePreferenceArg, argLen) == 0) {
                const char* preference = argv[i] + argLen;
                if (preference[0] != '\0') {
                    std::istringstream ss(preference);
                    std::string type;
                    while (std::getline(ss, type, ',')) {
                        if (strcmp(type.c_str(), "discrete") == 0) {
                            options.devicePreferences.push_back(
                                dawn_native::DeviceType::DiscreteGPU);
                        } else if (strcmp(type.c_str(), "integrated") == 0) {
                            options.devicePreferences.push_back(
                                dawn_native::DeviceType::IntegratedGPU);
                        } else if (strcmp(type.c_str(), "cpu") == 0) {
                            options.devicePreferences.push_back(dawn_native::DeviceType::CPU);
                        } else {
                            dawn::ErrorLog() << "Invalid device type preference: " << type;
                            UNREACHABLE();
                        }
                    }
                }
            }

            constexpr const char kWireTraceDirArg[] = "--wire-trace-dir=";
            argLen = sizeof(kWireTraceDirArg) - 1;
            if (strncmp(argv[i], kWireTraceDirArg, argLen) == 0) {
                const char* wireTraceDir = argv[i] + argLen;
                if (wireTraceDir[0] != '\0') {
                    const char* sep = GetPathSeparator();
                    options.wireTraceDir = wireTraceDir;
                    if (options.wireTraceDir.back() != *sep) {
                        options.wireTraceDir += sep;
                    }
                }
                continue;
            }

            if (strcmp("-h", argv[i]) == 0 || strcmp("--help", argv[i]) == 0) {
                dawn::InfoLog()
                    << "\n\nUsage: " << argv[0]
                    << " [GTEST_FLAGS...] [-w] [-d] [-c]\n"
                       "    [--enable-toggles=toggles] [--disable-toggles=toggles]\n"
                       "    [--adapter-vendor-id=x]"
                       " [--exclusive-device-type-preference=integrated,cpu,discrete]\n\n"
                       "  -w, --use-wire: Run the Dawn through the wire (defaults to no wire)\n"
                       "  -d, --enable-backend-validation: Enable backend validation (defaults"
                       " to disabled)\n"
                       "  -c, --begin-capture-on-startup: Begin debug capture on startup "
                       "(defaults to no capture)\n"
                       "  --enable-toggles: Comma-delimited list of Dawn toggles to enable.\n"
                       "    ex.) "
                       "skip_validation,use_tint_generator,disable_robustness,turn_off_vsync\n"
                       "  --disable-toggles: Comma-delimited list of Dawn toggles to disable\n"
                       "  --adapter-vendor-id: Select adapter by vendor id to run end2end tests"
                       "on multi-GPU systems \n"
                       "  --exclusive-device-type-preference: Comma-delimited list of preferred "
                       "device types. "
                       "For each backend, only adapters that match the first "
                       "available device type by default\n";
                continue;
            }
        }

        return options;
    }

    std::ostream& InstanceHolder::Options::Print(std::ostream& out,
                                                 dawn_native::Instance* instance) {
        out << "UseWire: " << (useWire ? "true" : "false")
            << "\nEnableBackendValidation: " << (enableBackendValidation ? "true" : "false")
            << "\nBeginCaptureOnStartup: " << (beginCaptureOnStartup ? "true" : "false");

        if (enabledToggles.size() > 0) {
            out << "\nEnabled Toggles\n";
            for (const std::string& toggle : enabledToggles) {
                const dawn_native::ToggleInfo* info = instance->GetToggleInfo(toggle.c_str());
                ASSERT(info != nullptr);
                out << " - " << info->name << ": " << info->description << "\n";
            }
        }
        if (disabledToggles.size() > 0) {
            out << "\nDisabled Toggles\n";
            for (const std::string& toggle : disabledToggles) {
                const dawn_native::ToggleInfo* info = instance->GetToggleInfo(toggle.c_str());
                ASSERT(info != nullptr);
                out << " - " << info->name << ": " << info->description << "\n";
            }
        }
        return out;
    }

    InstanceHolder::AdapterProperties::AdapterProperties(const wgpu::AdapterProperties& properties,
                                                         bool selected)
        : wgpu::AdapterProperties(properties), adapterName(properties.name), selected(selected) {
    }

    std::ostream& operator<<(std::ostream& out, const InstanceHolder::AdapterProperties& rhs) {
        std::ostringstream vendorId;
        std::ostringstream deviceId;
        vendorId << std::setfill('0') << std::uppercase << std::internal << std::hex << std::setw(4)
                 << rhs.vendorID;
        deviceId << std::setfill('0') << std::uppercase << std::internal << std::hex << std::setw(4)
                 << rhs.deviceID;

        // Preparing for outputting hex numbers
        out << std::showbase << std::hex << std::setfill('0') << std::setw(4)

            << " - \"" << rhs.adapterName << "\" - \"" << rhs.driverDescription << "\"\n"
            << "   type: " << AdapterTypeName(rhs.adapterType)
            << ", backend: " << BackendName(rhs.backendType) << "\n"
            << "   vendorId: 0x" << vendorId.str() << ", deviceId: 0x" << deviceId.str()
            << (rhs.selected ? " [Selected]" : "") << "\n";
        return out;
    }

    InstanceHolder::InstanceHolder(const Options& options)
        : mOptions(options), mInstance(new dawn_native::Instance()) {
        mInstance->EnableBackendValidation(options.enableBackendValidation);
        mInstance->EnableGPUBasedBackendValidation(options.enableBackendValidation);
        mInstance->EnableBeginCaptureOnStartup(options.beginCaptureOnStartup);

        if (options.useWire) {
            mC2sBuf = std::make_unique<utils::TerribleCommandBuffer>();
            mS2cBuf = std::make_unique<utils::TerribleCommandBuffer>();

            dawn_wire::WireServerDescriptor serverDesc = {};
            serverDesc.procs = &dawn_native::GetProcs();
            serverDesc.serializer = mS2cBuf.get();

            mWireServer.reset(new dawn_wire::WireServer(serverDesc));
            mC2sBuf->SetHandler(mWireServer.get());

            if (mOptions.wireTraceDir.length() > 0) {
                mWireServerTraceLayer.reset(
                    new WireServerTraceLayer(mOptions.wireTraceDir, mWireServer.get()));
                mC2sBuf->SetHandler(mWireServerTraceLayer.get());
            }

            dawn_wire::WireClientDescriptor clientDesc = {};
            clientDesc.serializer = mC2sBuf.get();

            mWireClient.reset(new dawn_wire::WireClient(clientDesc));
            mS2cBuf->SetHandler(mWireClient.get());
        }
        EnsureProcs();
    }

    void InstanceHolder::EnsureProcs() {
        if (mOptions.useWire) {
            dawnProcSetProcs(&dawn_wire::client::GetProcs());
        } else {
            dawnProcSetProcs(&dawn_native::GetProcs());
        }
    }

    InstanceHolder::~InstanceHolder() {
        if (mGlfwDidInit) {
            glfwTerminate();
        }
        dawnProcSetProcs(nullptr);
    }

    void InstanceHolder::DiscoverDefaultAdapters() {
        mInstance->DiscoverDefaultAdapters();

#ifdef DAWN_ENABLE_BACKEND_OPENGL
        GLFWwindow* openGLWindow =
            EnsureGLFWwindow(wgpu::BackendType::OpenGL, 400, 400, "Dawn OpenGL window");
        if (openGLWindow != nullptr) {
            glfwMakeContextCurrent(openGLWindow);
            dawn_native::opengl::AdapterDiscoveryOptions adapterOptions;
            adapterOptions.getProc = reinterpret_cast<void* (*)(const char*)>(glfwGetProcAddress);
            mInstance->DiscoverAdapters(&adapterOptions);
        }

        GLFWwindow* openGLESWindow =
            EnsureGLFWwindow(wgpu::BackendType::OpenGLES, 400, 400, "Dawn OpenGLES test window");
        if (openGLESWindow != nullptr) {
            glfwMakeContextCurrent(openGLESWindow);
            dawn_native::opengl::AdapterDiscoveryOptionsES adapterOptionsES;
            adapterOptionsES.getProc = reinterpret_cast<void* (*)(const char*)>(glfwGetProcAddress);
            mInstance->DiscoverAdapters(&adapterOptionsES);
        }
#endif  // DAWN_ENABLE_BACKEND_OPENGL
    }

    GLFWwindow* InstanceHolder::EnsureGLFWwindow(wgpu::BackendType backendType,
                                                 uint32_t width,
                                                 uint32_t height,
                                                 const char* name) {
        if (!mGlfwDidInit) {
            if (!glfwInit()) {
                return nullptr;
            }
            mGlfwDidInit = true;
        }

        auto it = mGlfwWindows.find(backendType);
        if (it != mGlfwWindows.end()) {
            return it->second;
        }

        utils::SetupGLFWWindowHintsForBackend(backendType);
        GLFWwindow* window = glfwCreateWindow(width, height, name, nullptr, nullptr);
        if (window == nullptr) {
            return nullptr;
        }

        mGlfwWindows[backendType] = window;
        return window;
    }

    GLFWwindow* InstanceHolder::GetGLFWwindow(wgpu::BackendType backendType) {
        return mGlfwWindows.at(backendType);
    }

    std::vector<InstanceHolder::AdapterProperties>
    InstanceHolder::ComputeSelectedAdapterProperties() {
        std::vector<InstanceHolder::AdapterProperties> adapterProperties;

        // Get the first available preferred device type.
        dawn_native::DeviceType preferredDeviceType = static_cast<dawn_native::DeviceType>(-1);
        bool hasDevicePreference = false;
        for (dawn_native::DeviceType devicePreference : mOptions.devicePreferences) {
            for (const dawn_native::Adapter& adapter : mInstance->GetAdapters()) {
                wgpu::AdapterProperties properties;
                adapter.GetProperties(&properties);

                if (adapter.GetDeviceType() == devicePreference) {
                    preferredDeviceType = devicePreference;
                    hasDevicePreference = true;
                    break;
                }
            }
            if (hasDevicePreference) {
                break;
            }
        }

        std::set<std::pair<wgpu::BackendType, std::string>> adapterNameSet;
        for (const dawn_native::Adapter& adapter : mInstance->GetAdapters()) {
            wgpu::AdapterProperties properties;
            adapter.GetProperties(&properties);

            // The adapter is selected if:
            bool selected = false;
            if (mOptions.hasDefaultVendorIdFilter) {
                // It matches the vendor id, if present.
                selected = mOptions.defaultVendorIdFilter == properties.vendorID;

                if (!mOptions.devicePreferences.empty()) {
                    dawn::WarningLog()
                        << "Vendor ID filter provided. Ignoring device type preference.";
                }
            } else if (hasDevicePreference) {
                // There is a device preference and:
                selected =
                    // The device type matches the first available preferred type for that backend,
                    // if present.
                    (adapter.GetDeviceType() == preferredDeviceType) ||
                    // Always select Unknown OpenGL adapters if we don't want a CPU adapter.
                    // OpenGL will usually be unknown because we can't query the device type.
                    // If we ever have Swiftshader GL (unlikely), we could set the DeviceType
                    // properly.
                    (preferredDeviceType != dawn_native::DeviceType::CPU &&
                     adapter.GetDeviceType() == dawn_native::DeviceType::Unknown &&
                     properties.backendType == wgpu::BackendType::OpenGL) ||
                    // Always select the Null backend. There are few tests on this backend, and they
                    // run quickly. This is temporary as to not lose coverage. We can group it with
                    // Swiftshader as a CPU adapter when we have Swiftshader tests.
                    (properties.backendType == wgpu::BackendType::Null);
            } else {
                // No vendor id or device preference was provided (select all).
                selected = true;
            }

            // In Windows Remote Desktop sessions we may be able to discover multiple adapters that
            // have the same name and backend type. We will just choose one adapter from them in our
            // tests.
            const auto adapterTypeAndName =
                std::make_pair(properties.backendType, std::string(properties.name));
            if (adapterNameSet.find(adapterTypeAndName) == adapterNameSet.end()) {
                adapterNameSet.insert(adapterTypeAndName);
                adapterProperties.emplace_back(properties, selected);
            }
        }

        return adapterProperties;
    }

    std::unique_ptr<InstanceHolder::ScopedWireTrace> InstanceHolder::BeginScopedWireTrace(
        std::string name) {
        if (mWireServerTraceLayer.get() != nullptr) {
            return static_cast<WireServerTraceLayer*>(mWireServerTraceLayer.get())
                ->BeginScopedWireTrace(name);
        } else {
            return std::make_unique<ScopedWireTrace>();
        }
    }

    std::pair<wgpu::Device, WGPUDevice> InstanceHolder::CreateDevice(
        dawn_native::Adapter* backendAdapter,
        const dawn_native::DeviceDescriptor& deviceDescriptor) {
        WGPUDevice backendDevice = backendAdapter->CreateDevice(&deviceDescriptor);
        ASSERT(backendDevice != nullptr);

        if (mOptions.useWire) {
            auto reservation = mWireClient->ReserveDevice();
            mWireServer->InjectDevice(backendDevice, reservation.id, reservation.generation);

            dawn_native::GetProcs().deviceRelease(backendDevice);
            return std::make_pair(wgpu::Device::Acquire(reservation.device), backendDevice);
        }

        return std::make_pair(wgpu::Device::Acquire(backendDevice), backendDevice);
    }

    void InstanceHolder::FlushWire() {
        if (mOptions.useWire) {
            bool C2SFlushed = mC2sBuf->Flush();
            bool S2CFlushed = mS2cBuf->Flush();
            ASSERT(C2SFlushed);
            ASSERT(S2CFlushed);
        }
    }

}  // namespace utils
