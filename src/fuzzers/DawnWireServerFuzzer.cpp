// Copyright 2019 The Dawn Authors
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

#include "DawnWireServerFuzzer.h"

#include "common/Assert.h"
#include "common/SystemUtils.h"
#include "dawn/dawn_proc.h"
#include "dawn/webgpu_cpp.h"
#include "dawn_native/DawnNative.h"
#include "dawn_native/ErrorInjector.h"
#include "dawn_wire/WireServer.h"

#include <fstream>
#include <vector>

namespace {

    class DevNull : public dawn_wire::CommandSerializer {
      public:
        void* GetCmdSpace(size_t size) override {
            if (size > buf.size()) {
                buf.resize(size);
            }
            return buf.data();
        }
        bool Flush() override {
            return true;
        }

      private:
        std::vector<char> buf;
    };

    WGPUProcDeviceCreateSwapChain sOriginalDeviceCreateSwapChain = nullptr;

    const char* sInjectedErrorTestcaseDir = nullptr;
    uint64_t sOutputFileNumber = 0;

    WGPUSwapChain ErrorDeviceCreateSwapChain(WGPUDevice device, const WGPUSwapChainDescriptor*) {
        WGPUSwapChainDescriptor desc;
        desc.nextInChain = nullptr;
        desc.label = nullptr;
        // A 0 implementation will trigger a swapchain creation error.
        desc.implementation = 0;
        return sOriginalDeviceCreateSwapChain(device, &desc);
    }

}  // namespace

int DawnWireServerFuzzer::Initialize(int* argc, char*** argv) {
    ASSERT(argc != nullptr && argv != nullptr);

    // Save the original argument count for iterating.
    int argcIn = *argc;

    // Take references to argv and argc so we can rewrite the arguments.
    char**& argvRef = *argv;
    int& argcRef = *argc;

    // Reset argc to one. argv[0] is the same.
    argcRef = 1;

    for (int i = 1; i < argcIn; ++i) {
        constexpr const char kInjectedErrorTestcaseDirArg[] = "--injected-error-testcase-dir=";
        if (strstr(argvRef[i], kInjectedErrorTestcaseDirArg) == argvRef[i]) {
            sInjectedErrorTestcaseDir = argvRef[i] + strlen(kInjectedErrorTestcaseDirArg);
            continue;
        }

        // Move any unconsumed arguments to the next slot in the output array.
        argvRef[argcRef++] = argvRef[i];
    }

    return 0;
}

int DawnWireServerFuzzer::Run(const uint8_t* data,
                              size_t size,
                              MakeDeviceFn MakeDevice,
                              bool supportsErrorInjection) {
    const bool generatingInjectedErrorTestcases =
        supportsErrorInjection && sInjectedErrorTestcaseDir != nullptr;

    if (supportsErrorInjection) {
        dawn_native::EnableErrorInjector();

        // Clear the error injector since it has the previous run's call counts.
        dawn_native::ClearErrorInjector();

        // If we're not generating the error testcases, use the last bytes as the injected error
        // index.
        if (!generatingInjectedErrorTestcases && size >= sizeof(uint64_t)) {
            dawn_native::InjectErrorAt(
                *reinterpret_cast<const uint64_t*>(data + size - sizeof(uint64_t)));
            size -= sizeof(uint64_t);
        }
    }

    DawnProcTable procs = dawn_native::GetProcs();

    // Swapchains receive a pointer to an implementation. The fuzzer will pass garbage in so we
    // intercept calls to create swapchains and make sure they always return error swapchains.
    // This is ok for fuzzing because embedders of dawn_wire would always define their own
    // swapchain handling.
    sOriginalDeviceCreateSwapChain = procs.deviceCreateSwapChain;
    procs.deviceCreateSwapChain = ErrorDeviceCreateSwapChain;

    dawnProcSetProcs(&procs);

    std::unique_ptr<dawn_native::Instance> instance = std::make_unique<dawn_native::Instance>();
    wgpu::Device device = MakeDevice(instance.get());
    if (!device) {
        // If we're generating injected error testcases, we should never fail device creation.
        ASSERT(!generatingInjectedErrorTestcases && supportsErrorInjection);
        return 0;
    }

    DevNull devNull;
    dawn_wire::WireServerDescriptor serverDesc = {};
    serverDesc.device = device.Get();
    serverDesc.procs = &procs;
    serverDesc.serializer = &devNull;

    std::unique_ptr<dawn_wire::WireServer> wireServer(new dawn_wire::WireServer(serverDesc));

    wireServer->HandleCommands(reinterpret_cast<const char*>(data), size);

    // Fake waiting for all previous commands before destroying the server.
    device.Tick();

    // Destroy the server before the device because it needs to free all objects.
    wireServer = nullptr;
    device = nullptr;
    instance = nullptr;

    if (generatingInjectedErrorTestcases) {
        std::string basepath = sInjectedErrorTestcaseDir;
        const char* sep = GetPathSeparator();
        if (basepath.back() != *sep) {
            basepath += sep;
        }

        const uint64_t injectedCallCount = dawn_native::AcquireErrorInjectorCallCount();

        auto WriteTestcase = [&](uint64_t i) {
            std::ofstream outFile(
                basepath + "injected_error_testcase_" + std::to_string(sOutputFileNumber++),
                std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
            outFile.write(reinterpret_cast<const char*>(data), size);
            outFile.write(reinterpret_cast<const char*>(&i), sizeof(i));
        };

        for (uint64_t i = 0; i < injectedCallCount; ++i) {
            WriteTestcase(i);
        }

        // Also add a testcase where the injected error is so large no errors should occur.
        WriteTestcase(std::numeric_limits<uint64_t>::max());
    }

    return 0;
}
