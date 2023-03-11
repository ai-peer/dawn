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

#include "dawn/native/interpreter/DeviceInterpreter.h"

#include "dawn/native/BindGroupLayout.h"
#include "dawn/native/ChainUtils.h"
#include "dawn/native/ErrorData.h"
#include "dawn/native/PipelineLayout.h"
#include "dawn/native/interpreter/BindGroupInterpreter.h"
#include "dawn/native/interpreter/BufferInterpreter.h"
#include "dawn/native/interpreter/CommandBufferInterpreter.h"
#include "dawn/native/interpreter/ComputePipelineInterpreter.h"
#include "dawn/native/interpreter/QueueInterpreter.h"
#include "dawn/native/interpreter/RenderPipelineInterpreter.h"
#include "dawn/native/interpreter/ShaderModuleInterpreter.h"

namespace dawn::native::interpreter {

// static
ResultOrError<Ref<Device>> Device::Create(AdapterBase* adapter,
                                          const UnpackedPtr<DeviceDescriptor>& descriptor,
                                          const TogglesState& userProvidedToggles) {
    Ref<Device> device = AcquireRef(new Device(adapter, descriptor, userProvidedToggles));
    DAWN_TRY(device->Initialize(descriptor));
    return device;
}

Device::Device(AdapterBase* adapter,
               const UnpackedPtr<DeviceDescriptor>& descriptor,
               const TogglesState& userProvidedToggles)
    : DeviceBase(adapter, descriptor, userProvidedToggles) {}

Device::~Device() = default;

MaybeError Device::Initialize(const UnpackedPtr<DeviceDescriptor>& descriptor) {
    return DeviceBase::Initialize(Queue::Create(this, &descriptor->defaultQueue));
}

MaybeError Device::TickImpl() {
    return DAWN_UNIMPLEMENTED_ERROR("interpreterDevice::TickImpl");
}

ResultOrError<Ref<CommandBufferBase>> Device::CreateCommandBuffer(
    CommandEncoder* encoder,
    const CommandBufferDescriptor* descriptor) {
    return CommandBuffer::Create(encoder, descriptor);
}

MaybeError Device::CopyFromStagingToBufferImpl(BufferBase* source,
                                               uint64_t sourceOffset,
                                               BufferBase* destination,
                                               uint64_t destinationOffset,
                                               uint64_t size) {
    ToBackend(destination)
        ->GetMemory()
        .CopyFrom(destinationOffset, ToBackend(source)->GetMemory(), sourceOffset, size);
    return {};
}

MaybeError Device::CopyFromStagingToTextureImpl(const BufferBase* source,
                                                const TextureDataLayout& src,
                                                const TextureCopy& dst,
                                                const Extent3D& copySizePixels) {
    return DAWN_UNIMPLEMENTED_ERROR("interpreterDevice::CopyFromStagingToTexture");
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

ResultOrError<Ref<BindGroupBase>> Device::CreateBindGroupImpl(
    const BindGroupDescriptor* descriptor) {
    return BindGroup::Create(this, descriptor);
}

ResultOrError<Ref<BindGroupLayoutInternalBase>> Device::CreateBindGroupLayoutImpl(
    const BindGroupLayoutDescriptor* descriptor) {
    return AcquireRef(new BindGroupLayoutInternalBase(this, descriptor));
}

ResultOrError<Ref<PipelineLayoutBase>> Device::CreatePipelineLayoutImpl(
    const UnpackedPtr<PipelineLayoutDescriptor>& descriptor) {
    return AcquireRef(new PipelineLayoutBase(this, descriptor));
}

Ref<ComputePipelineBase> Device::CreateUninitializedComputePipelineImpl(
    const UnpackedPtr<ComputePipelineDescriptor>& descriptor) {
    return ComputePipeline::CreateUninitialized(this, descriptor);
}

Ref<RenderPipelineBase> Device::CreateUninitializedRenderPipelineImpl(
    const UnpackedPtr<RenderPipelineDescriptor>& descriptor) {
    return RenderPipeline::CreateUninitialized(this, descriptor);
}

ResultOrError<Ref<ShaderModuleBase>> Device::CreateShaderModuleImpl(
    const UnpackedPtr<ShaderModuleDescriptor>& descriptor,
    ShaderModuleParseResult* parseResult,
    OwnedCompilationMessages* compilationMessages) {
    return ShaderModule::Create(this, descriptor, parseResult, compilationMessages);
}

ResultOrError<Ref<BufferBase>> Device::CreateBufferImpl(
    const UnpackedPtr<BufferDescriptor>& descriptor) {
    return Buffer::Create(this, descriptor);
}

ResultOrError<Ref<TextureBase>> Device::CreateTextureImpl(
    const UnpackedPtr<TextureDescriptor>& descriptor) {
    return DAWN_UNIMPLEMENTED_ERROR("interpreterDevice::CreateTexture");
}

ResultOrError<Ref<TextureViewBase>> Device::CreateTextureViewImpl(
    TextureBase* texture,
    const TextureViewDescriptor* descriptor) {
    return DAWN_UNIMPLEMENTED_ERROR("interpreterDevice::CreateTextureView");
}

ResultOrError<Ref<SamplerBase>> Device::CreateSamplerImpl(const SamplerDescriptor* descriptor) {
    return DAWN_UNIMPLEMENTED_ERROR("interpreterDevice::CreateSampler");
}

ResultOrError<Ref<QuerySetBase>> Device::CreateQuerySetImpl(const QuerySetDescriptor* descriptor) {
    return DAWN_UNIMPLEMENTED_ERROR("interpreterDevice::CreateQuerySet");
}

ResultOrError<Ref<SwapChainBase>> Device::CreateSwapChainImpl(
    Surface* surface,
    SwapChainBase* previousSwapChain,
    const SwapChainDescriptor* descriptor) {
    return DAWN_UNIMPLEMENTED_ERROR("interpreterDevice::CreateSwapChain");
}

ResultOrError<wgpu::TextureUsage> Device::GetSupportedSurfaceUsageImpl(
    const Surface* surface) const {
    return DAWN_UNIMPLEMENTED_ERROR("interpreterDevice::GetSupportedSurfaceUsageImpl");
}

void Device::DestroyImpl() {}

}  // namespace dawn::native::interpreter
