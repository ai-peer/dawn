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

#ifndef UTILS_INSTANCEHOLDER_H_
#define UTILS_INSTANCEHOLDER_H_

#include "dawn/webgpu_cpp.h"
#include "dawn_native/DawnNative.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

namespace dawn_native {
    class Instance;
}  // namespace dawn_native

namespace dawn_wire {
    class CommandHandler;
    class WireClient;
    class WireServer;
}  // namespace dawn_wire

namespace utils {

    class TerribleCommandBuffer;

    class InstanceHolder {
      public:
        struct ScopedWireTrace {
            virtual ~ScopedWireTrace() = default;
        };

        struct Options {
            static Options FromCommandLine(int argc, char** argv);
            std::ostream& Print(std::ostream& out, dawn_native::Instance* instance);

            bool useWire = false;
            bool enableBackendValidation = false;
            bool beginCaptureOnStartup = false;
            bool hasDefaultVendorIdFilter = false;
            uint32_t defaultVendorIdFilter;
            std::string wireTraceDir;
            std::vector<std::string> enabledToggles;
            std::vector<std::string> disabledToggles;
            std::vector<dawn_native::DeviceType> devicePreferences;
        };

        struct AdapterProperties : wgpu::AdapterProperties {
            AdapterProperties(const wgpu::AdapterProperties& properties, bool selected);

            friend std::ostream& operator<<(std::ostream& out, const AdapterProperties& rhs);

            std::string adapterName;
            bool selected;

          private:
            // This may be temporary, so it is copied into |adapterName| and made private.
            using wgpu::AdapterProperties::name;
        };

        explicit InstanceHolder(const Options& options);
        ~InstanceHolder();

        dawn_native::Instance* GetInstance() {
            return mInstance.get();
        }

        const Options& GetOptions() const {
            return mOptions;
        }

        void EnsureProcs();

        // Discovers default adapters on the instance, and also discovers
        // creates GLFW windows for OpenGL / OpenGL ES and discovers adapters there.
        void DiscoverDefaultAdapters();

        // Compute selected adapter properties based on discovered adapters and |mOptions|.
        std::vector<AdapterProperties> ComputeSelectedAdapterProperties();

        GLFWwindow* EnsureGLFWwindow(wgpu::BackendType backendType,
                                     uint32_t width,
                                     uint32_t height,
                                     const char* name);
        GLFWwindow* GetGLFWwindow(wgpu::BackendType backendType);

        // If using the wire, injects the devices into the wire. Otherwise does nothing.
        // Returns a pair of the client device and backend device.
        // This function takes ownership of |backendDevice|.
        std::pair<wgpu::Device, WGPUDevice> RegisterDevice(WGPUDevice backendDevice);

        std::unique_ptr<InstanceHolder::ScopedWireTrace> BeginScopedWireTrace(std::string name);

        void FlushWire();

      private:
        Options mOptions;
        std::unique_ptr<dawn_native::Instance> mInstance;

        bool mGlfwDidInit = false;
        std::map<wgpu::BackendType, GLFWwindow*> mGlfwWindows;

        // Things used to set up running Dawn through the Wire.
        std::unique_ptr<utils::TerribleCommandBuffer> mC2sBuf;
        std::unique_ptr<utils::TerribleCommandBuffer> mS2cBuf;
        std::unique_ptr<dawn_wire::CommandHandler> mWireServerTraceLayer;
        std::unique_ptr<dawn_wire::WireServer> mWireServer;
        std::unique_ptr<dawn_wire::WireClient> mWireClient;
    };

}  // namespace utils

#endif  // UTILS_INSTANCEHOLDER_H_
