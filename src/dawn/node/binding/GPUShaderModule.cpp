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

#include "src/dawn/node/binding/GPUShaderModule.h"

#include <memory>
#include <utility>
#include <vector>

#include "src/dawn/node/binding/GPUCompilationInfo.h"
#include "src/dawn/node/utils/Debug.h"

namespace wgpu::binding {

////////////////////////////////////////////////////////////////////////////////
// wgpu::bindings::GPUShaderModule
////////////////////////////////////////////////////////////////////////////////
GPUShaderModule::GPUShaderModule(wgpu::ShaderModule shader, std::shared_ptr<AsyncRunner> async)
    : shader_(std::move(shader)), async_(std::move(async)) {}

interop::Promise<interop::Interface<interop::GPUCompilationInfo>>
GPUShaderModule::getCompilationInfo(Napi::Env env) {
    using Promise = interop::Promise<interop::Interface<interop::GPUCompilationInfo>>;

    struct Context {
        Napi::Env env;
        Promise promise;
        wgpu::ShaderModule shader;
        AsyncTask task;
    };
    auto ctx = new Context{env, Promise(env, PROMISE_INFO), shader_, AsyncTask(async_)};
    auto promise = ctx->promise;

    shader_.GetCompilationInfo(
        [](WGPUCompilationInfoRequestStatus status, WGPUCompilationInfo const* compilationInfo,
           void* userdata) {
            auto c = std::unique_ptr<Context>(static_cast<Context*>(userdata));
            c->promise.Resolve(interop::GPUCompilationInfo::Create<GPUCompilationInfo>(
                c->env, c->env, c->shader, compilationInfo));
        },
        ctx);

    return promise;
}

std::string GPUShaderModule::getLabel(Napi::Env) {
    UNIMPLEMENTED();
}

void GPUShaderModule::setLabel(Napi::Env, std::string value) {
    UNIMPLEMENTED();
}

}  // namespace wgpu::binding
