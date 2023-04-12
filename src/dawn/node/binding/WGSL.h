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

#ifndef SRC_DAWN_NODE_BINDING_WGSL_H_
#define SRC_DAWN_NODE_BINDING_WGSL_H_

#include <optional>
#include <string>

#include "dawn/native/DawnNative.h"
#include "dawn/webgpu_cpp.h"
#include "src/dawn/node/interop/Napi.h"
#include "src/dawn/node/interop/WebGPU.h"

namespace tint {
class Program;
}
namespace tint::sem {
class Function;
class GlobalVariable;
}  // namespace tint::sem

namespace tint::type {
class Array;
class Atomic;
class DepthTexture;
class Matrix;
class MultisampledTexture;
class SampledTexture;
class Sampler;
class Struct;
class StructMember;
class Texture;
class Type;
class Vector;
}  // namespace tint::type

namespace wgpu::binding {

class WGSLBindPoint final : public interop::WGSLBindPoint {
  public:
    WGSLBindPoint(const tint::Program*, const tint::sem::GlobalVariable*);
    ~WGSLBindPoint();

    interop::GPUIndex32 getGroup(Napi::Env) override;
    interop::GPUIndex32 getBinding(Napi::Env) override;
    std::string getName(Napi::Env) override;
    interop::Interface<interop::WGSLTypeBase> getType(Napi::Env) override;

  private:
    tint::Program const* const program_;
    tint::sem::GlobalVariable const* const global_;
};

class WGSLBindGroup final : public interop::WGSLBindGroup {
  public:
    WGSLBindGroup(const tint::Program*, const tint::sem::Function*, uint32_t group);
    ~WGSLBindGroup();

    bool has(Napi::Env, interop::GPUIndex32) override;
    std::vector<interop::GPUIndex32> keys(Napi::Env) override;
    std::vector<interop::Interface<interop::WGSLBindPoint>> values(Napi::Env) override;
    interop::Interface<interop::WGSLBindPoint> get(Napi::Env, interop::GPUIndex32) override;
    interop::GPUIndex32 getGroup(Napi::Env) override;

  private:
    tint::Program const* const program_;
    tint::sem::Function const* const fn_;
    uint32_t const group_;
};

class WGSLBindGroups final : public interop::WGSLBindGroups {
  public:
    WGSLBindGroups(const tint::Program*, const tint::sem::Function*);
    ~WGSLBindGroups();

    bool has(Napi::Env, interop::GPUIndex32) override;
    std::vector<interop::GPUIndex32> keys(Napi::Env) override;
    std::vector<interop::Interface<interop::WGSLBindGroup>> values(Napi::Env) override;
    interop::Interface<interop::WGSLBindGroup> get(Napi::Env, interop::GPUIndex32) override;

  private:
    tint::Program const* const program_;
    tint::sem::Function const* const fn_;
};

class WGSLEntryPoint final : public interop::WGSLEntryPoint {
  public:
    WGSLEntryPoint(const tint::Program* p, const tint::sem::Function* f);
    ~WGSLEntryPoint();

    interop::WGSLShaderStage getStage(Napi::Env) override;
    interop::Interface<interop::WGSLBindGroups> getBindgroups(Napi::Env) override;
    std::string getName(Napi::Env) override;

  private:
    tint::Program const* const program_;
    tint::sem::Function const* const fn_;
};

class WGSLEntryPoints final : public interop::WGSLEntryPoints {
  public:
    WGSLEntryPoints(const tint::Program* p);
    ~WGSLEntryPoints();

    bool has(Napi::Env, std::string) override;
    std::vector<std::string> keys(Napi::Env) override;
    std::vector<interop::Interface<interop::WGSLEntryPoint>> values(Napi::Env) override;
    interop::Interface<interop::WGSLEntryPoint> get(Napi::Env, std::string) override;

  private:
    void build(Napi::Env);
    tint::Program const* const program_;
};

}  // namespace wgpu::binding

#endif  // SRC_DAWN_NODE_BINDING_WGSL_H_
