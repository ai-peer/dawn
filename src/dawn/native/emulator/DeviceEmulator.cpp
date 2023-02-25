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

#include "dawn/native/emulator/DeviceEmulator.h"

#include "dawn/native/BindGroupLayout.h"
#include "dawn/native/CommandBuffer.h"
#include "dawn/native/ErrorData.h"
#include "dawn/native/PipelineLayout.h"
#include "dawn/native/emulator/AdapterEmulator.h"
#include "dawn/native/emulator/BindGroupEmulator.h"
#include "dawn/native/emulator/BufferEmulator.h"
#include "dawn/native/emulator/ComputePipelineEmulator.h"
#include "dawn/native/emulator/QueueEmulator.h"
#include "dawn/native/emulator/RenderPipelineEmulator.h"
#include "dawn/native/emulator/ShaderModuleEmulator.h"

namespace dawn::native::emulator {

// static
ResultOrError<Ref<Device>> Device::Create(Adapter* adapter,
                                          const DeviceDescriptor* descriptor,
                                          const TripleStateTogglesSet& userProvidedToggles) {
    Ref<Device> device = AcquireRef(new Device(adapter, descriptor, userProvidedToggles));
    DAWN_TRY(device->Initialize(descriptor));
    return device;
}

Device::Device(Adapter* adapter,
               const DeviceDescriptor* descriptor,
               const TripleStateTogglesSet& userProvidedToggles)
    : DeviceBase(adapter, descriptor, userProvidedToggles) {}

Device::~Device() = default;

MaybeError Device::Initialize(const DeviceDescriptor* descriptor) {
    return DeviceBase::Initialize(Queue::Create(this, &descriptor->defaultQueue));
}

MaybeError Device::TickImpl() {
    return DAWN_UNIMPLEMENTED_ERROR("emulator::Device::TickImpl");
}

ResultOrError<Ref<CommandBufferBase>> Device::CreateCommandBuffer(
    CommandEncoder* encoder,
    const CommandBufferDescriptor* descriptor) {
    return AcquireRef(new CommandBufferBase(encoder, descriptor));
}

MaybeError Device::CopyFromStagingToBufferImpl(BufferBase* source,
                                               uint64_t sourceOffset,
                                               BufferBase* destination,
                                               uint64_t destinationOffset,
                                               uint64_t size) {
    ToBackend(destination)
        ->Get()
        .CopyFrom(destinationOffset, ToBackend(source)->Get(), sourceOffset, size);
    return {};
}

MaybeError Device::CopyFromStagingToTextureImpl(const BufferBase* source,
                                                const TextureDataLayout& src,
                                                const TextureCopy& dst,
                                                const Extent3D& copySizePixels) {
    return DAWN_UNIMPLEMENTED_ERROR("emulator::Device::CopyFromStagingToTexture");
}

uint32_t Device::GetOptimalBytesPerRowAlignment() const {
    return 1;
}

uint64_t Device::GetOptimalBufferToTextureCopyOffsetAlignment() const {
    return 1;
}

float Device::GetTimestampPeriodInNS() const {
    return 1.f;
}

void Device::ForceEventualFlushOfCommands() {}

ResultOrError<Ref<BindGroupBase>> Device::CreateBindGroupImpl(
    const BindGroupDescriptor* descriptor) {
    return AcquireRef(new BindGroup(this, descriptor));
}

ResultOrError<Ref<BindGroupLayoutBase>> Device::CreateBindGroupLayoutImpl(
    const BindGroupLayoutDescriptor* descriptor,
    PipelineCompatibilityToken pipelineCompatibilityToken) {
    return AcquireRef(new BindGroupLayoutBase(this, descriptor, pipelineCompatibilityToken));
}

ResultOrError<Ref<PipelineLayoutBase>> Device::CreatePipelineLayoutImpl(
    const PipelineLayoutDescriptor* descriptor) {
    return AcquireRef(new PipelineLayoutBase(this, descriptor));
}

Ref<ComputePipelineBase> Device::CreateUninitializedComputePipelineImpl(
    const ComputePipelineDescriptor* descriptor) {
    return AcquireRef(new ComputePipeline(this, descriptor));
}

Ref<RenderPipelineBase> Device::CreateUninitializedRenderPipelineImpl(
    const RenderPipelineDescriptor* descriptor) {
    return AcquireRef(new RenderPipeline(this, descriptor));
}

ResultOrError<Ref<ShaderModuleBase>> Device::CreateShaderModuleImpl(
    const ShaderModuleDescriptor* descriptor,
    ShaderModuleParseResult* parseResult,
    OwnedCompilationMessages* compilationMessages) {
    Ref<ShaderModule> module = AcquireRef(new ShaderModule(this, descriptor));
    DAWN_TRY(module->Initialize(parseResult, compilationMessages));
    return module;
}

ResultOrError<Ref<BufferBase>> Device::CreateBufferImpl(const BufferDescriptor* descriptor) {
    return Buffer::Create(this, descriptor);
}

ResultOrError<Ref<TextureBase>> Device::CreateTextureImpl(const TextureDescriptor* descriptor) {
    return DAWN_UNIMPLEMENTED_ERROR("emulator::Device::CreateTexture");
}

ResultOrError<Ref<TextureViewBase>> Device::CreateTextureViewImpl(
    TextureBase* texture,
    const TextureViewDescriptor* descriptor) {
    return DAWN_UNIMPLEMENTED_ERROR("emulator::Device::CreateTextureView");
}

ResultOrError<Ref<SamplerBase>> Device::CreateSamplerImpl(const SamplerDescriptor* descriptor) {
    return DAWN_UNIMPLEMENTED_ERROR("emulator::Device::CreateSampler");
}

ResultOrError<Ref<QuerySetBase>> Device::CreateQuerySetImpl(const QuerySetDescriptor* descriptor) {
    return DAWN_UNIMPLEMENTED_ERROR("emulator::Device::CreateQuerySet");
}

ResultOrError<Ref<SwapChainBase>> Device::CreateSwapChainImpl(
    const SwapChainDescriptor* descriptor) {
    return DAWN_UNIMPLEMENTED_ERROR("emulator::Device::CreateSwapChain");
}

ResultOrError<Ref<NewSwapChainBase>> Device::CreateSwapChainImpl(
    Surface* surface,
    NewSwapChainBase* previousSwapChain,
    const SwapChainDescriptor* descriptor) {
    return DAWN_UNIMPLEMENTED_ERROR("emulator::Device::CreateSwapChain");
}

ResultOrError<ExecutionSerial> Device::CheckAndUpdateCompletedSerials() {
    return DAWN_UNIMPLEMENTED_ERROR("emulator::Device::CheckAndUpdateCompletedSerials");
}

void Device::DestroyImpl() {}

MaybeError Device::WaitForIdleForDestruction() {
    return DAWN_UNIMPLEMENTED_ERROR("emulator::Device::WaitForIdleForDestruction");
}

bool Device::HasPendingCommands() const {
    return false;
}

}  // namespace dawn::native::emulator
