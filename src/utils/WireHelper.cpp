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

#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace utils {

    namespace {

        class WireServerTraceLayer : public dawn_wire::CommandHandler {
            struct ScopedWireTraceImpl : ScopedWireTrace {
                ScopedWireTraceImpl(std::ofstream* file) : file(file) {
                }
                ~ScopedWireTraceImpl() override {
                    file->close();
                }

                std::ofstream* file;
            };

          public:
            WireServerTraceLayer(const char* dir, dawn_wire::CommandHandler* handler)
                : dawn_wire::CommandHandler(), mDir(dir), mHandler(handler) {
                const char* sep = GetPathSeparator();
                if (mDir.back() != *sep) {
                    mDir += sep;
                }
            }

            std::unique_ptr<ScopedWireTrace> BeginScopedWireTrace(const char* name) {
                std::string filename = name;
                // Replace slashes in gtest names with underscores so everything is in one
                // directory.
                std::replace(filename.begin(), filename.end(), '/', '_');
                std::replace(filename.begin(), filename.end(), '\\', '_');

                // Prepend the filename with the directory.
                filename = mDir + filename;

                ASSERT(!mFile.is_open());
                mFile.open(filename,
                           std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
                return std::unique_ptr<ScopedWireTrace>(new ScopedWireTraceImpl(&mFile));
            }

            const volatile char* HandleCommands(const volatile char* commands,
                                                size_t size) override {
                if (mFile.is_open()) {
                    mFile.write(const_cast<const char*>(commands), size);
                }
                return mHandler->HandleCommands(commands, size);
            }

          private:
            std::string mDir;
            dawn_wire::CommandHandler* mHandler;
            std::ofstream mFile;
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

            std::unique_ptr<ScopedWireTrace> BeginScopedWireTrace(const char* name) override {
                return std::make_unique<ScopedWireTrace>();
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
            explicit WireHelperProxy(const char* wireTraceDir) {
                mC2sBuf = std::make_unique<utils::TerribleCommandBuffer>();
                mS2cBuf = std::make_unique<utils::TerribleCommandBuffer>();

                dawn_wire::WireServerDescriptor serverDesc = {};
                serverDesc.procs = &dawn_native::GetProcs();
                serverDesc.serializer = mS2cBuf.get();

                mWireServer.reset(new dawn_wire::WireServer(serverDesc));
                mC2sBuf->SetHandler(mWireServer.get());

                if (wireTraceDir != nullptr && strlen(wireTraceDir) > 0) {
                    mWireServerTraceLayer.reset(
                        new WireServerTraceLayer(wireTraceDir, mWireServer.get()));
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

            std::unique_ptr<ScopedWireTrace> BeginScopedWireTrace(const char* name) override {
                if (mWireServerTraceLayer) {
                    return mWireServerTraceLayer->BeginScopedWireTrace(name);
                } else {
                    return std::make_unique<ScopedWireTrace>();
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

    std::unique_ptr<WireHelper> CreateWireHelper(bool useWire, const char* wireTraceDir) {
        if (useWire) {
            return std::unique_ptr<WireHelper>(new WireHelperProxy(wireTraceDir));
        } else {
            return std::unique_ptr<WireHelper>(new WireHelperDirect());
        }
    }

    WireHelper::~WireHelper() {
        dawnProcSetProcs(nullptr);
    }

    // WireHelper::WireHelper(bool useWire, const char* wireTraceDir) : mUseWire(useWire) {
    //     if (mUseWire) {
    //         mC2sBuf = std::make_unique<utils::TerribleCommandBuffer>();
    //         mS2cBuf = std::make_unique<utils::TerribleCommandBuffer>();

    //         dawn_wire::WireServerDescriptor serverDesc = {};
    //         serverDesc.procs = &dawn_native::GetProcs();
    //         serverDesc.serializer = mS2cBuf.get();

    //         mWireServer.reset(new dawn_wire::WireServer(serverDesc));
    //         mC2sBuf->SetHandler(mWireServer.get());

    //         if (wireTraceDir != nullptr && strlen(wireTraceDir) > 0) {
    //             mWireServerTraceLayer.reset(
    //                 new WireServerTraceLayer(wireTraceDir, mWireServer.get()));
    //             mC2sBuf->SetHandler(mWireServerTraceLayer.get());
    //         }

    //         dawn_wire::WireClientDescriptor clientDesc = {};
    //         clientDesc.serializer = mC2sBuf.get();

    //         mWireClient.reset(new dawn_wire::WireClient(clientDesc));
    //         mS2cBuf->SetHandler(mWireClient.get());
    //         dawnProcSetProcs(&dawn_wire::client::GetProcs());
    //         return;
    //     }
    //     dawnProcSetProcs(&dawn_native::GetProcs());
    // }

    // WireHelper::~WireHelper() {
    //     dawnProcSetProcs(nullptr);
    // }

    // std::unique_ptr<ScopedWireTrace> WireHelper::BeginScopedWireTrace(const char* name) {
    //     if (mWireServerTraceLayer) {
    //         return static_cast<WireServerTraceLayer*>(mWireServerTraceLayer.get())
    //             ->BeginScopedWireTrace(name);
    //     } else {
    //         return std::make_unique<ScopedWireTrace>();
    //     }
    // }

    // std::pair<wgpu::Device, WGPUDevice> WireHelper::RegisterDevice(WGPUDevice backendDevice) {
    //     ASSERT(backendDevice != nullptr);

    //     if (mUseWire) {
    //         auto reservation = mWireClient->ReserveDevice();
    //         mWireServer->InjectDevice(backendDevice, reservation.id, reservation.generation);
    //         dawn_native::GetProcs().deviceRelease(backendDevice);

    //         return std::make_pair(wgpu::Device::Acquire(reservation.device), backendDevice);
    //     }

    //     return std::make_pair(wgpu::Device::Acquire(backendDevice), backendDevice);
    // }

    // bool WireHelper::FlushClient() {
    //     return !mUseWire || mC2sBuf->Flush();
    // }

    // bool WireHelper::FlushServer() {
    //     return !mUseWire || mS2cBuf->Flush();
    // }

}  // namespace utils
