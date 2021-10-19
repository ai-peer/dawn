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

#include "dawn/dawn_proc.h"
#include "src/dawn_node/binding/Flags.h"
#include "src/dawn_node/binding/GPU.h"

namespace {
    void SetFlag(const Napi::CallbackInfo& info) {
        if (info.Length() != 1) {
            std::cerr << "SetFlag expecting only one argument" << std::endl;
            return;
        }

        std::tuple<std::string> arg;
        auto r = wgpu::interop::FromJS(info, arg);
        if (r != wgpu::interop::Success) {
            std::cerr << "SetFlag failed to extract argument" << std::endl;
            return;
        }

        std::string arg0 = std::get<0>(arg);
        const size_t sep_index = arg0.find("=");
        if (sep_index == std::string::npos) {
            std::cerr << "SetFlag expected argument format is <key>=<value>";
            return;
        }

        wgpu::binding::Flags::Set(arg0.substr(0, sep_index), arg0.substr(sep_index + 1));
    }
}  // namespace

// Initialize() initializes the Dawn node module, registering all the WebGPU
// types into the global object, and adding the 'gpu' property on the exported
// object.
Napi::Object Initialize(Napi::Env env, Napi::Object exports) {
    // Begin by setting the Dawn procedure function pointers.
    dawnProcSetProcs(&dawn_native::GetProcs());

    // Register all the interop types
    wgpu::interop::Initialize(env);

    // Construct a wgpu::interop::GPU interface, implemented by
    // wgpu::bindings::GPU. This will be the 'gpu' field of exported object.
    auto gpu = wgpu::interop::GPU::Create<wgpu::binding::GPU>(env);
    exports.Set(Napi::String::New(env, "gpu"), gpu);

    // Export function for setting dawn_node configuration flags
    exports.Set(Napi::String::New(env, "setFlag"), Napi::Function::New<SetFlag>(env));

    return exports;
}

NODE_API_MODULE(addon, Initialize)
