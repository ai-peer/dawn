// Copyright 2018 The Dawn Authors
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

#include "dawn_native/metal/DeviceMTL.h"

#include "dawn_native/BackendConnection.h"
#include "dawn_native/BindGroup.h"
#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/DynamicUploader.h"
#include "dawn_native/RenderPassDescriptor.h"
#include "dawn_native/metal/BufferMTL.h"
#include "dawn_native/metal/CommandBufferMTL.h"
#include "dawn_native/metal/ComputePipelineMTL.h"
#include "dawn_native/metal/InputStateMTL.h"
#include "dawn_native/metal/PipelineLayoutMTL.h"
#include "dawn_native/metal/QueueMTL.h"
#include "dawn_native/metal/RenderPipelineMTL.h"
#include "dawn_native/metal/ResourceUploader.h"
#include "dawn_native/metal/SamplerMTL.h"
#include "dawn_native/metal/ShaderModuleMTL.h"
#include "dawn_native/metal/SwapChainMTL.h"
#include "dawn_native/metal/TextureMTL.h"

#include <type_traits>

namespace dawn_native { namespace metal {

    Device::Device(AdapterBase* adapter, id<MTLDevice> mtlDevice)
        : DeviceBase(adapter),
          mMtlDevice([mtlDevice retain]),
          mMapTracker(new MapRequestTracker(this)),
<<<<<<< HEAD
          mCompletedSerial(0) {
=======
          mResourceUploader(new ResourceUploader(this)) {
>>>>>>> [Not For Review] Revert to reproduce issue 101
        [mMtlDevice retain];
        mCommandQueue = [mMtlDevice newCommandQueue];
    }

    Device::~Device() {
        // Wait for all commands to be finished so we can free resources SubmitPendingCommandBuffer
        // may not increment the pendingCommandSerial if there are no pending commands, so we can't
        // store the pendingSerial before SubmitPendingCommandBuffer then wait for it to be passed.
        // Instead we submit and wait for the serial before the next pendingCommandSerial.
        SubmitPendingCommandBuffer();
        while (GetCompletedCommandSerial() != mLastSubmittedSerial) {
            usleep(100);
        }
        Tick();

        [mPendingCommands release];
        mPendingCommands = nil;

        mMapTracker = nullptr;
        mResourceUploader = nullptr;

        [mCommandQueue release];
        mCommandQueue = nil;

        [mMtlDevice release];
        mMtlDevice = nil;
    }

    ResultOrError<BindGroupBase*> Device::CreateBindGroupImpl(
        const BindGroupDescriptor* descriptor) {
        return new BindGroup(this, descriptor);
    }
    ResultOrError<BindGroupLayoutBase*> Device::CreateBindGroupLayoutImpl(
        const BindGroupLayoutDescriptor* descriptor) {
        return new BindGroupLayout(this, descriptor);
    }
    ResultOrError<BufferBase*> Device::CreateBufferImpl(const BufferDescriptor* descriptor) {
        return new Buffer(this, descriptor);
    }
    CommandBufferBase* Device::CreateCommandBuffer(CommandBufferBuilder* builder) {
        return new CommandBuffer(builder);
    }
    ResultOrError<ComputePipelineBase*> Device::CreateComputePipelineImpl(
        const ComputePipelineDescriptor* descriptor) {
        return new ComputePipeline(this, descriptor);
    }
    InputStateBase* Device::CreateInputState(InputStateBuilder* builder) {
        return new InputState(builder);
    }
    ResultOrError<PipelineLayoutBase*> Device::CreatePipelineLayoutImpl(
        const PipelineLayoutDescriptor* descriptor) {
        return new PipelineLayout(this, descriptor);
    }
    RenderPassDescriptorBase* Device::CreateRenderPassDescriptor(
        RenderPassDescriptorBuilder* builder) {
        return new RenderPassDescriptor(builder);
    }
    ResultOrError<QueueBase*> Device::CreateQueueImpl() {
        return new Queue(this);
    }
    ResultOrError<RenderPipelineBase*> Device::CreateRenderPipelineImpl(
        const RenderPipelineDescriptor* descriptor) {
        return new RenderPipeline(this, descriptor);
    }
    ResultOrError<SamplerBase*> Device::CreateSamplerImpl(const SamplerDescriptor* descriptor) {
        return new Sampler(this, descriptor);
    }
    ResultOrError<ShaderModuleBase*> Device::CreateShaderModuleImpl(
        const ShaderModuleDescriptor* descriptor) {
        return new ShaderModule(this, descriptor);
    }
    SwapChainBase* Device::CreateSwapChain(SwapChainBuilder* builder) {
        return new SwapChain(builder);
    }
    ResultOrError<TextureBase*> Device::CreateTextureImpl(const TextureDescriptor* descriptor) {
        return new Texture(this, descriptor);
    }
    ResultOrError<TextureViewBase*> Device::CreateTextureViewImpl(
        TextureBase* texture,
        const TextureViewDescriptor* descriptor) {
        return new TextureView(texture, descriptor);
    }

    Serial Device::GetCompletedCommandSerial() const {
        static_assert(std::is_same<Serial, uint64_t>::value, "");
        return mCompletedSerial.load();
    }

    Serial Device::GetLastSubmittedCommandSerial() const {
        return mLastSubmittedSerial;
    }

    Serial Device::GetPendingCommandSerial() const {
        return mLastSubmittedSerial + 1;
    }

    void Device::TickImpl() {
<<<<<<< HEAD
        Serial completedSerial = GetCompletedCommandSerial();

        mDynamicUploader->Tick(completedSerial);
        mMapTracker->Tick(completedSerial);
=======
        mResourceUploader->Tick(mCompletedSerial);
        mMapTracker->Tick(mCompletedSerial);
>>>>>>> [Not For Review] Revert to reproduce issue 101

        if (mPendingCommands != nil) {
            SubmitPendingCommandBuffer();
        } else if (completedSerial == mLastSubmittedSerial) {
            // If there's no GPU work in flight we still need to artificially increment the serial
            // so that CPU operations waiting on GPU completion can know they don't have to wait.
            mCompletedSerial++;
            mLastSubmittedSerial++;
        }
    }

    id<MTLDevice> Device::GetMTLDevice() {
        return mMtlDevice;
    }

    id<MTLCommandBuffer> Device::GetPendingCommandBuffer() {
        if (mPendingCommands == nil) {
            mPendingCommands = [mCommandQueue commandBuffer];
            [mPendingCommands retain];
        }
        return mPendingCommands;
    }

    void Device::SubmitPendingCommandBuffer() {
        if (mPendingCommands == nil) {
            return;
        }

        mLastSubmittedSerial++;

        // Replace mLastSubmittedCommands with the mutex held so we avoid races between the
        // schedule handler and this code.
        {
            std::lock_guard<std::mutex> lock(mLastSubmittedCommandsMutex);
            [mLastSubmittedCommands release];
            mLastSubmittedCommands = mPendingCommands;
        }

        // Ok, ObjC blocks are weird. My understanding is that local variables are captured by
        // value so this-> works as expected. However it is unclear how members are captured, (are
        // they captured using this-> or by value?). To be safe we copy members to local variables
        // to ensure they are captured "by value".

        // Free mLastSubmittedCommands as soon as it is scheduled so that it doesn't hold
        // references to its resources. Make a local copy of pendingCommands first so it is
        // captured "by-value" by the block.
        id<MTLCommandBuffer> pendingCommands = mPendingCommands;

        [mPendingCommands addScheduledHandler:^(id<MTLCommandBuffer>) {
            // This is DRF because we hold the mutex for mLastSubmittedCommands and pendingCommands
            // is a local value (and not the member itself).
            std::lock_guard<std::mutex> lock(mLastSubmittedCommandsMutex);
            if (this->mLastSubmittedCommands == pendingCommands) {
                [this->mLastSubmittedCommands release];
                this->mLastSubmittedCommands = nil;
            }
        }];

        // Update the completed serial once the completed handler is fired. Make a local copy of
        // mLastSubmittedSerial so it is captured by value.
        Serial pendingSerial = mLastSubmittedSerial;
        [mPendingCommands addCompletedHandler:^(id<MTLCommandBuffer>) {
            ASSERT(pendingSerial > mCompletedSerial.load());
            this->mCompletedSerial = pendingSerial;
        }];

        [mPendingCommands commit];
        mPendingCommands = nil;
    }

    MapRequestTracker* Device::GetMapTracker() const {
        return mMapTracker.get();
    }

    ResourceUploader* Device::GetResourceUploader() const {
        return mResourceUploader.get();
    }

    ResultOrError<std::unique_ptr<StagingBufferBase>> Device::CreateStagingBuffer(size_t size) {
        return DAWN_UNIMPLEMENTED_ERROR("Device unable to create staging buffer.");
    }

    MaybeError Device::CopyFromStagingToBuffer(StagingBufferBase* source,
                                               uint32_t sourceOffset,
                                               BufferBase* destination,
                                               uint32_t destinationOffset,
                                               uint32_t size) {
        return DAWN_UNIMPLEMENTED_ERROR("Device unable to copy from staging buffer.");
    }

<<<<<<< HEAD
    void Device::WaitForCommandsToBeScheduled() {
        SubmitPendingCommandBuffer();
        [mLastSubmittedCommands waitUntilScheduled];
    }

=======
>>>>>>> [Not For Review] Revert to reproduce issue 101
}}  // namespace dawn_native::metal
