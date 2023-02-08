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

#include "dawn/native/error/DeviceError.h"

#include "dawn/native/BindGroup.h"
#include "dawn/native/BindGroupLayout.h"
#include "dawn/native/Buffer.h"
#include "dawn/native/CommandBuffer.h"
#include "dawn/native/ComputePipeline.h"
#include "dawn/native/PipelineLayout.h"
#include "dawn/native/QuerySet.h"
#include "dawn/native/Queue.h"
#include "dawn/native/RenderPipeline.h"
#include "dawn/native/Sampler.h"
#include "dawn/native/ShaderModule.h"
#include "dawn/native/SwapChain.h"
#include "dawn/native/Texture.h"

namespace dawn::native::error {

class ErrorQueue final : public QueueBase {
  public:
    ErrorQueue(Device* device, const QueueDescriptor* descriptor) : QueueBase(device, descriptor) {}

  private:
    ~ErrorQueue() override {}
    MaybeError SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) override {
        return {};
    }
};

Ref<Device> Device::Create(AdapterBase* adapter, const DeviceDescriptor* descriptor) {
    // Preserve any labels that were given, because that can be useful for debugging, but otherwise
    // force error devices to use a default descriptor. Also preserve device lost callbacks because
    // error devices call them immediately.
    DeviceDescriptor errorDescriptor = {};
    errorDescriptor.label = descriptor->label;
    errorDescriptor.deviceLostCallback = descriptor->deviceLostCallback;
    errorDescriptor.deviceLostUserdata = descriptor->deviceLostUserdata;

    const TogglesState nullToggles(ToggleStage::Device);
    Ref<Device> device = AcquireRef(new Device(adapter, &errorDescriptor, nullToggles));
    return device;
}

Device::~Device() {
    Destroy();
}

MaybeError Device::Initialize() {
    const QueueDescriptor queueDescriptor = {};
    return DeviceBase::Initialize(AcquireRef(new ErrorQueue(this, &queueDescriptor)));
}

ResultOrError<Ref<BindGroupBase>> Device::CreateBindGroupImpl(
    const BindGroupDescriptor* descriptor) {
    return AcquireRef(BindGroupBase::MakeError(this, descriptor ? descriptor->label : nullptr));
}
ResultOrError<Ref<BindGroupLayoutBase>> Device::CreateBindGroupLayoutImpl(
    const BindGroupLayoutDescriptor* descriptor,
    PipelineCompatibilityToken) {
    return AcquireRef(
        BindGroupLayoutBase::MakeError(this, descriptor ? descriptor->label : nullptr));
}
ResultOrError<Ref<BufferBase>> Device::CreateBufferImpl(const BufferDescriptor* descriptor) {
    return AcquireRef(BufferBase::MakeError(this, descriptor));
}
ResultOrError<Ref<CommandBufferBase>> Device::CreateCommandBuffer(
    CommandEncoder*,
    const CommandBufferDescriptor* descriptor) {
    return AcquireRef(CommandBufferBase::MakeError(this, descriptor ? descriptor->label : nullptr));
}
Ref<ComputePipelineBase> Device::CreateUninitializedComputePipelineImpl(
    const ComputePipelineDescriptor* descriptor) {
    return AcquireRef(
        ComputePipelineBase::MakeError(this, descriptor ? descriptor->label : nullptr));
}
ResultOrError<Ref<PipelineLayoutBase>> Device::CreatePipelineLayoutImpl(
    const PipelineLayoutDescriptor* descriptor) {
    return AcquireRef(
        PipelineLayoutBase::MakeError(this, descriptor ? descriptor->label : nullptr));
}
ResultOrError<Ref<QuerySetBase>> Device::CreateQuerySetImpl(const QuerySetDescriptor* descriptor) {
    return AcquireRef(QuerySetBase::MakeError(this, descriptor));
}
Ref<RenderPipelineBase> Device::CreateUninitializedRenderPipelineImpl(
    const RenderPipelineDescriptor* descriptor) {
    return AcquireRef(
        RenderPipelineBase::MakeError(this, descriptor ? descriptor->label : nullptr));
}
ResultOrError<Ref<SamplerBase>> Device::CreateSamplerImpl(const SamplerDescriptor* descriptor) {
    return AcquireRef(SamplerBase::MakeError(this, descriptor ? descriptor->label : nullptr));
}
ResultOrError<Ref<ShaderModuleBase>> Device::CreateShaderModuleImpl(
    const ShaderModuleDescriptor* descriptor,
    ShaderModuleParseResult*,
    OwnedCompilationMessages*) {
    return ShaderModuleBase::MakeError(this, descriptor ? descriptor->label : nullptr);
}
ResultOrError<Ref<SwapChainBase>> Device::CreateSwapChainImpl(Surface*,
                                                              SwapChainBase*,
                                                              const SwapChainDescriptor*) {
    return AcquireRef(SwapChainBase::MakeError(this));
}
ResultOrError<Ref<TextureBase>> Device::CreateTextureImpl(const TextureDescriptor* descriptor) {
    return AcquireRef(TextureBase::MakeError(this, descriptor));
}
ResultOrError<Ref<TextureViewBase>> Device::CreateTextureViewImpl(
    TextureBase*,
    const TextureViewDescriptor* descriptor) {
    return AcquireRef(TextureViewBase::MakeError(this, descriptor ? descriptor->label : nullptr));
}

ResultOrError<ExecutionSerial> Device::CheckAndUpdateCompletedSerials() {
    return GetLastSubmittedCommandSerial();
}

ResultOrError<wgpu::TextureUsage> Device::GetSupportedSurfaceUsageImpl(
    const Surface* surface) const {
    return wgpu::TextureUsage::RenderAttachment;
}
}  // namespace dawn::native::error
