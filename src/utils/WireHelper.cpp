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

#include "utils/WireHelper.h"

#include "common/Assert.h"
#include "common/Log.h"
#include "common/SystemUtils.h"
#include "dawn/dawn_proc.h"
#include "dawn_native/DawnNative.h"
#include "dawn_wire/WireClient.h"
#include "dawn_wire/WireServer.h"
#include "utils/TerribleCommandBuffer.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace utils {

    namespace {

        class WireServerTraceLayer : public dawn_wire::CommandHandler {
          public:
            WireServerTraceLayer(dawn_wire::CommandHandler* handler,
                                 const char* dir,
                                 const char* injectedDir)
                : dawn_wire::CommandHandler(),
                  mHandler(handler),
                  mDir(dir),
                  mInjectedDir(injectedDir) {
                const char* sep = GetPathSeparator();
                if (mDir.size() > 0 && mDir.back() != *sep) {
                    mDir += sep;
                }

                if (mInjectedDir.size() > 0 && mInjectedDir.back() != *sep) {
                    mInjectedDir += sep;
                }
            }

            void BeginWireTrace(const char* name) {
                std::string filename = name;
                // Replace slashes in gtest names with underscores so everything is in one
                // directory.
                std::replace(filename.begin(), filename.end(), '/', '_');
                std::replace(filename.begin(), filename.end(), '\\', '_');

                if (mDir.size() > 0) {
                    ASSERT(!mTraceFile.is_open());
                    mTraceFile.open(mDir + filename, std::ios_base::out | std::ios_base::binary |
                                                         std::ios_base::trunc);
                }

                if (mInjectedDir.size() > 0) {
                    ASSERT(!mInjectionTraceFile.is_open());
                    mInjectionTraceFile.open(mInjectedDir + filename, std::ios_base::out |
                                                                          std::ios_base::binary |
                                                                          std::ios_base::trunc);

                    // Write the initial 8 bytes. This means the fuzzer should never inject an
                    // error.
                    const uint64_t injectedErrorIndex = 0xFFFF'FFFF'FFFF'FFFF;
                    mInjectionTraceFile.write(reinterpret_cast<const char*>(&injectedErrorIndex),
                                              sizeof(injectedErrorIndex));
                }
            }

            const volatile char* HandleCommands(const volatile char* commands,
                                                size_t size) override {
                if (mTraceFile.is_open()) {
                    mTraceFile.write(const_cast<const char*>(commands), size);
                }
                if (mInjectionTraceFile.is_open()) {
                    mInjectionTraceFile.write(const_cast<const char*>(commands), size);
                }
                return mHandler->HandleCommands(commands, size);
            }

          private:
            dawn_wire::CommandHandler* mHandler;
            std::string mDir;
            std::string mInjectedDir;
            std::ofstream mTraceFile;
            std::ofstream mInjectionTraceFile;
        };

        class WireHelperDirect : public WireHelper {
          public:
            WireHelperDirect() {
                dawnProcSetProcs(&dawn_native::GetProcs());
            }

            std::pair<wgpu::Device, WGPUDevice> RegisterDevice(WGPUDevice backendDevice) override {
                ASSERT(backendDevice != nullptr);
                return std::make_pair(wgpu::Device::Acquire(backendDevice), backendDevice);
            }

            void BeginWireTrace(const char* name) override {
            }

            bool FlushClient() override {
                return true;
            }

            bool FlushServer() override {
                return true;
            }
        };

        class WireHelperProxy : public WireHelper {
          public:
            WireHelperProxy(const char* wireTraceDir, const char* wireInjectedTraceDir) {
                mC2sBuf = std::make_unique<utils::TerribleCommandBuffer>();
                mS2cBuf = std::make_unique<utils::TerribleCommandBuffer>();

                dawn_wire::WireServerDescriptor serverDesc = {};
                serverDesc.procs = &dawn_native::GetProcs();
                serverDesc.serializer = mS2cBuf.get();

                mWireServer.reset(new dawn_wire::WireServer(serverDesc));
                mC2sBuf->SetHandler(mWireServer.get());

                if ((wireTraceDir != nullptr && strlen(wireTraceDir) > 0) ||
                    (wireInjectedTraceDir != nullptr && strlen(wireInjectedTraceDir) > 0)) {
                    mWireServerTraceLayer.reset(new WireServerTraceLayer(
                        mWireServer.get(), wireTraceDir, wireInjectedTraceDir));
                    mC2sBuf->SetHandler(mWireServerTraceLayer.get());
                }

                dawn_wire::WireClientDescriptor clientDesc = {};
                clientDesc.serializer = mC2sBuf.get();

                mWireClient.reset(new dawn_wire::WireClient(clientDesc));
                mS2cBuf->SetHandler(mWireClient.get());
                dawnProcSetProcs(&dawn_wire::client::GetProcs());
            }

            std::pair<wgpu::Device, WGPUDevice> RegisterDevice(WGPUDevice backendDevice) override {
                ASSERT(backendDevice != nullptr);

                auto reservation = mWireClient->ReserveDevice();
                mWireServer->InjectDevice(backendDevice, reservation.id, reservation.generation);
                dawn_native::GetProcs().deviceRelease(backendDevice);

                return std::make_pair(wgpu::Device::Acquire(reservation.device), backendDevice);
            }

            void BeginWireTrace(const char* name) override {
                if (mWireServerTraceLayer) {
                    return mWireServerTraceLayer->BeginWireTrace(name);
                }
            }

            bool FlushClient() override {
                return mC2sBuf->Flush();
            }

            bool FlushServer() override {
                return mS2cBuf->Flush();
            }

          private:
            std::unique_ptr<utils::TerribleCommandBuffer> mC2sBuf;
            std::unique_ptr<utils::TerribleCommandBuffer> mS2cBuf;
            std::unique_ptr<WireServerTraceLayer> mWireServerTraceLayer;
            std::unique_ptr<dawn_wire::WireServer> mWireServer;
            std::unique_ptr<dawn_wire::WireClient> mWireClient;
        };

    }  // anonymous namespace

    std::unique_ptr<WireHelper> CreateWireHelper(bool useWire,
                                                 const char* wireTraceDir,
                                                 const char* wireInjectedTraceDir) {
        if (useWire) {
            return std::unique_ptr<WireHelper>(
                new WireHelperProxy(wireTraceDir, wireInjectedTraceDir));
        } else {
            return std::unique_ptr<WireHelper>(new WireHelperDirect());
        }
    }

    WireHelper::~WireHelper() {
        dawnProcSetProcs(nullptr);
    }

}  // namespace utils
