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

#ifndef SRC_DAWN_NATIVE_ERROR_DEVICEERROR_H_
#define SRC_DAWN_NATIVE_ERROR_DEVICEERROR_H_

#include "dawn/native/Device.h"

class Adapter;
class BindGroupBase;
class BindGroupLayoutBase;
class BufferBase;
class CommandBufferBase;
class ComputePipelineBase;
class PipelineLayoutBase;
class QuerySetBase;
class Queue;
class RenderPipelineBase;
class SamplerBase;
class ShaderModuleBase;
class SwapChainBase;
class TextureBase;
class TextureViewBase;

namespace dawn::native::error {

class Device final : public DeviceBase {
  public:
    static Ref<Device> Create(AdapterBase* adapter, const DeviceDescriptor* descriptor);
    ~Device() override;

    MaybeError Initialize();

  private:
    using DeviceBase::DeviceBase;

    ResultOrError<Ref<BindGroupBase>> CreateBindGroupImpl(
        const BindGroupDescriptor* descriptor) override;
    ResultOrError<Ref<BindGroupLayoutBase>> CreateBindGroupLayoutImpl(
        const BindGroupLayoutDescriptor* descriptor,
        PipelineCompatibilityToken pipelineCompatibilityToken) override;
    ResultOrError<Ref<BufferBase>> CreateBufferImpl(const BufferDescriptor* descriptor) override;
    ResultOrError<Ref<CommandBufferBase>> CreateCommandBuffer(
        CommandEncoder* encoder,
        const CommandBufferDescriptor* descriptor) override;
    Ref<ComputePipelineBase> CreateUninitializedComputePipelineImpl(
        const ComputePipelineDescriptor* descriptor) override;
    ResultOrError<Ref<PipelineLayoutBase>> CreatePipelineLayoutImpl(
        const PipelineLayoutDescriptor* descriptor) override;
    ResultOrError<Ref<QuerySetBase>> CreateQuerySetImpl(
        const QuerySetDescriptor* descriptor) override;
    Ref<RenderPipelineBase> CreateUninitializedRenderPipelineImpl(
        const RenderPipelineDescriptor* descriptor) override;
    ResultOrError<Ref<SamplerBase>> CreateSamplerImpl(const SamplerDescriptor* descriptor) override;
    ResultOrError<Ref<ShaderModuleBase>> CreateShaderModuleImpl(
        const ShaderModuleDescriptor* descriptor,
        ShaderModuleParseResult* parseResult,
        OwnedCompilationMessages* compilationMessages) override;
    ResultOrError<Ref<SwapChainBase>> CreateSwapChainImpl(
        Surface* surface,
        SwapChainBase* previousSwapChain,
        const SwapChainDescriptor* descriptor) override;
    ResultOrError<Ref<TextureBase>> CreateTextureImpl(const TextureDescriptor* descriptor) override;
    ResultOrError<Ref<TextureViewBase>> CreateTextureViewImpl(
        TextureBase* texture,
        const TextureViewDescriptor* descriptor) override;

    ResultOrError<ExecutionSerial> CheckAndUpdateCompletedSerials() override;

    ResultOrError<wgpu::TextureUsage> GetSupportedSurfaceUsageImpl(
        const Surface* surface) const override;

    void DestroyImpl() override {}
    MaybeError WaitForIdleForDestruction() override { return {}; }
    bool HasPendingCommands() const override { return false; }

    MaybeError TickImpl() override { return {}; }

    MaybeError CopyFromStagingToBufferImpl(BufferBase* source,
                                           uint64_t sourceOffset,
                                           BufferBase* destination,
                                           uint64_t destinationOffset,
                                           uint64_t size) override {
        return {};
    }
    MaybeError CopyFromStagingToTextureImpl(const BufferBase* source,
                                            const TextureDataLayout& src,
                                            const TextureCopy& dst,
                                            const Extent3D& copySizePixels) override {
        return {};
    }

    uint32_t GetOptimalBytesPerRowAlignment() const override { return 1; }
    uint64_t GetOptimalBufferToTextureCopyOffsetAlignment() const override { return 1; }
    float GetTimestampPeriodInNS() const override { return 1.0f; }
    void ForceEventualFlushOfCommands() override {}
};

}  // namespace dawn::native::error

#endif  // SRC_DAWN_NATIVE_ERROR_DEVICEERROR_H_
