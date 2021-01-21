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

#ifndef UTILS_WIREHELPER_H_
#define UTILS_WIREHELPER_H_

#include "dawn/webgpu_cpp.h"

#include <cstdint>
#include <memory>

namespace dawn_wire {
    class CommandHandler;
    class WireClient;
    class WireServer;
}  // namespace dawn_wire

namespace utils {

    class TerribleCommandBuffer;

    struct ScopedWireTrace {
        virtual ~ScopedWireTrace() = default;
    };

    class WireHelper {
      public:
        explicit WireHelper(bool useWire, const char* wireTraceDir = nullptr);
        ~WireHelper();

        // If using the wire, injects the devices into the wire. Otherwise does nothing.
        // Returns a pair of the client device and backend device.
        // This function takes ownership of |backendDevice|.
        std::pair<wgpu::Device, WGPUDevice> RegisterDevice(WGPUDevice backendDevice);

        std::unique_ptr<ScopedWireTrace> BeginScopedWireTrace(const char* name);

        bool FlushClient();
        bool FlushServer();

      private:
        const bool mUseWire;

        // Things used to set up running Dawn through the Wire.
        std::unique_ptr<utils::TerribleCommandBuffer> mC2sBuf;
        std::unique_ptr<utils::TerribleCommandBuffer> mS2cBuf;
        std::unique_ptr<dawn_wire::CommandHandler> mWireServerTraceLayer;
        std::unique_ptr<dawn_wire::WireServer> mWireServer;
        std::unique_ptr<dawn_wire::WireClient> mWireClient;
    };

}  // namespace utils

#endif  // UTILS_WIREHELPER_H_
