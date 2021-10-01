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

#include "src/dawn_node/binding/GPU.h"

#include "src/dawn_node/binding/GPUAdapter.h"

#include <cstdlib>

namespace {
    std::string getEnvVar(const char* varName) {
#if defined(_WIN32)
        // Use _dupenv_s to avoid unsafe warnings about std::getenv
        char* value = nullptr;
        _dupenv_s(&value, nullptr, varName);
        if (value) {
            std::string result = value;
            free(value);
            return result;
        }
        return "";
#else
        return std::getenv(varName);
#endif
    }
}  // namespace

namespace wgpu { namespace binding {

    ////////////////////////////////////////////////////////////////////////////////
    // wgpu::bindings::GPU
    ////////////////////////////////////////////////////////////////////////////////
    GPU::GPU() {
        // TODO: Disable in 'release'
        instance_.EnableBackendValidation(true);
        instance_.SetBackendValidationLevel(dawn_native::BackendValidationLevel::Full);

        instance_.DiscoverDefaultAdapters();
    }

    interop::Promise<std::optional<interop::Interface<interop::GPUAdapter>>> GPU::requestAdapter(
        Napi::Env env,
        std::optional<interop::GPURequestAdapterOptions> options) {
        auto promise =
            interop::Promise<std::optional<interop::Interface<interop::GPUAdapter>>>(env);

        if (options.has_value() && options->forceFallbackAdapter) {
            // Software adapters are not currently supported.
            promise.Resolve({});
            return promise;
        }

        auto adapters = instance_.GetAdapters();
        if (adapters.empty()) {
            promise.Resolve({});
            return promise;
        }

#if defined(_WIN32)
        constexpr auto DEFAULT_BACKEND_TYPE = wgpu::BackendType::D3D12;
#elif defined(__linux__)
        constexpr auto DEFAULT_BACKEND_TYPE = wgpu::BackendType::Vulkan;
#elif defined(__APPLE__)
        constexpr auto DEFAULT_BACKEND_TYPE = wgpu::BackendType::Metal;
#else
#    error "Unsupported platform"
#endif

        auto targetBackendType = DEFAULT_BACKEND_TYPE;

        // Check for override from env var
        std::string envVar = getEnvVar("DAWNNODE_BACKEND");
        if (!envVar.empty()) {
            if (envVar == "Vulkan") {
                targetBackendType = wgpu::BackendType::Vulkan;
            }
        }

        // Default to first adapter if we don't find a match
        size_t adapterIndex = 0;
        for (size_t i = 0; i < adapters.size(); ++i) {
            wgpu::AdapterProperties props;
            adapters[i].GetProperties(&props);
            if (props.backendType == targetBackendType) {
                adapterIndex = i;
                break;
            }
        }

        auto adapter = GPUAdapter::Create<GPUAdapter>(env, adapters[adapterIndex]);
        promise.Resolve(std::optional<interop::Interface<interop::GPUAdapter>>(adapter));
        return promise;
    }

}}  // namespace wgpu::binding
