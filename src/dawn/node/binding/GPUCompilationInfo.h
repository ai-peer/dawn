// Copyright 2023 The Dawn Authors
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

#ifndef SRC_DAWN_NODE_BINDING_GPUCOMPILATIONINFO_H_
#define SRC_DAWN_NODE_BINDING_GPUCOMPILATIONINFO_H_

#include <memory>
#include <string>

#include "dawn/native/DawnNative.h"
#include "dawn/webgpu_cpp.h"
#include "src/dawn/node/binding/AsyncRunner.h"
#include "src/dawn/node/interop/Napi.h"
#include "src/dawn/node/interop/WebGPU.h"

namespace tint {
class Program;
}

namespace wgpu::binding {

// GPUCompilationInfo is an implementation of interop::GPUCompilationInfo.
class GPUCompilationInfo final : public interop::GPUCompilationInfo {
  public:
    GPUCompilationInfo(Napi::Env env,
                       wgpu::ShaderModule shaderModule,
                       WGPUCompilationInfo const* compilationInfo);

    interop::Interface<interop::WGSLEntryPoints> getEntrypoints(Napi::Env) override;
    interop::FrozenArray<interop::Interface<interop::GPUCompilationMessage>> getMessages(
        Napi::Env) override;

  private:
    wgpu::ShaderModule module;
    std::vector<Napi::ObjectReference> messages;
    const tint::Program* program = nullptr;
};

}  // namespace wgpu::binding

#endif  // SRC_DAWN_NODE_BINDING_GPUCOMPILATIONINFO_H_
