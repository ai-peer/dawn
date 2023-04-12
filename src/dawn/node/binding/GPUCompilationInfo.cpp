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

#include "src/dawn/node/binding/GPUCompilationInfo.h"

#include "src/dawn/node/binding/WGSL.h"
#include "src/dawn/node/utils/Debug.h"

namespace wgpu::binding {

namespace {

struct GPUCompilationMessage : public interop::GPUCompilationMessage {
    WGPUCompilationMessage message;

    explicit GPUCompilationMessage(const WGPUCompilationMessage& m) : message(m) {}
    std::string getMessage(Napi::Env) override { return message.message; }
    interop::GPUCompilationMessageType getType(Napi::Env) override {
        switch (message.type) {
            case WGPUCompilationMessageType_Error:
                return interop::GPUCompilationMessageType::kError;
            case WGPUCompilationMessageType_Warning:
                return interop::GPUCompilationMessageType::kWarning;
            case WGPUCompilationMessageType_Info:
                return interop::GPUCompilationMessageType::kInfo;
            default:
                UNIMPLEMENTED();
        }
    }
    uint64_t getLineNum(Napi::Env) override { return message.lineNum; }
    uint64_t getLinePos(Napi::Env) override { return message.linePos; }
    uint64_t getOffset(Napi::Env) override { return message.offset; }
    uint64_t getLength(Napi::Env) override { return message.length; }
};

using Messages = std::vector<interop::Interface<interop::GPUCompilationMessage>>;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// wgpu::bindings::GPUCompilationInfo
////////////////////////////////////////////////////////////////////////////////

GPUCompilationInfo::GPUCompilationInfo(Napi::Env env,
                                       wgpu::ShaderModule shaderModule,
                                       WGPUCompilationInfo const* compilationInfo)
    : module(shaderModule) {
    messages.reserve(compilationInfo->messageCount);
    for (uint32_t i = 0; i < compilationInfo->messageCount; i++) {
        auto& msg = compilationInfo->messages[i];
        auto obj = interop::GPUCompilationMessage::Create<GPUCompilationMessage>(env, msg);
        messages.emplace_back(Napi::Persistent(Napi::Object(env, obj)));
    }
    program = reinterpret_cast<tint::Program*>(static_cast<uintptr_t>(compilationInfo->program));
}

interop::Interface<interop::WGSLEntryPoints> GPUCompilationInfo::getEntrypoints(Napi::Env env) {
    if (!program) {
        return {};
    }
    return interop::WGSLEntryPoints::Create<WGSLEntryPoints>(env, program);
}

Messages GPUCompilationInfo::getMessages(Napi::Env) {
    Messages out;
    out.reserve(messages.size());
    for (auto& msg : messages) {
        out.emplace_back(msg.Value());
    }
    return out;
}

}  // namespace wgpu::binding
