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

            ~WireServerTraceLayer() override {
                if (mFile.is_open()) {
                    mFile.close();
                    if (mInjectedDir.size() == 0) {
                        return;
                    }

                    // Acquire the number of injected errors.
                    const uint64_t injectedCallCount = dawn_native::AcquireErrorInjectorCallCount();

                    // For each injected error, create a file and write out the error index.
                    std::vector<std::ofstream> outputFiles(injectedCallCount);
                    for (uint64_t i = 0; i < injectedCallCount; ++i) {
                        outputFiles[i].open(
                            mInjectedDir + mFilename + "_injected_" + std::to_string(i),
                            std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
                        outputFiles[i].write(reinterpret_cast<const char*>(&i), sizeof(i));
                    }

                    // Read the original trace file and append the contents to all of the output
                    // files.
                    char buf[4096];
                    std::ifstream original(mDir + mFilename, std::ios_base::in | std::ios_base::binary);
                    do {
                        original.read(&buf[0], sizeof(buf));

                        for (uint64_t i = 0; i < injectedCallCount; ++i) {
                            outputFiles[i].write(&buf[0], original.gcount());
                        }

                    } while (original.gcount() > 0);
                }
            }

            void BeginWireTrace(const char* name) {
                mFilename = name;
                // Replace slashes in gtest names with underscores so everything is in one
                // directory.
                std::replace(mFilename.begin(), mFilename.end(), '/', '_');
                std::replace(mFilename.begin(), mFilename.end(), '\\', '_');

                // Prepend the filename with the directory.
                std::string path = mDir + mFilename;

                ASSERT(!mFile.is_open());
                mFile.open(path, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);

                if (mInjectedDir.size() == 0) {
                    return;
                }

                // To write injected error traces to the output directory, first enable the error
                // injector. The error injector will increment a counter for all callsites where
                // error injection is possible.
                dawn_native::EnableErrorInjector();

                // Clear the error injector since it may have a previous run's call counts.
                dawn_native::ClearErrorInjector();
            }

            const volatile char* HandleCommands(const volatile char* commands,
                                                size_t size) override {
                if (mFile.is_open()) {
                    mFile.write(const_cast<const char*>(commands), size);
                }
                return mHandler->HandleCommands(commands, size);
            }

          private:
            dawn_wire::CommandHandler* mHandler;
            std::string mDir;
            std::string mInjectedDir;
            std::string mFilename;
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

                if (wireTraceDir != nullptr && strlen(wireTraceDir) > 0) {
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
