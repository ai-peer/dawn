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

#include <IOKit/IOKitLib.h>
#include <unistd.h>

namespace dawn_native { namespace metal {

    namespace {
        struct PCIIDs {
            uint32_t vendorId;
            uint32_t deviceId;
        };

        // Queries the IO Registry to find the PCI device and vendor IDs of the MTLDevice.
        // The registry entry correponding to [device registryID] doesn't contain the exact PCI ids
        // because it corresponds to a driver. However its parent entry corresponds to the device
        // itself and has uint32_t "device-id" and "registry-id" keys. For example on a dual-GPU
        // MacBook Pro 2017 the IORegistry explorer shows the following tree (simplified here):
        //
        //  - PCI0@0
        //  | - AppleACPIPCI
        //  | | - IGPU@2 (type IOPCIDevice)
        //  | | | - IntelAccelerator (type IOGraphicsAccelerator2)
        //  | | - PEG0@1
        //  | | | - IOPP
        //  | | | | - GFX0@0 (type IOPCIDevice)
        //  | | | | | - AMDRadeonX4000_AMDBaffinGraphicsAccelerator (type IOGraphicsAccelerator2)
        //
        // [device registryID] is the ID for one of the IOGraphicsAccelerator2 and we can see that
        // their parent always is an IOPCIDevice that has properties for the device and vendor IDs.
        ResultOrError<PCIIDs> GetDeviceIORegistryPCIInfo(id<MTLDevice> device) {
            // Get a matching dictionary for the IOGraphicsAccelerator2
            CFMutableDictionaryRef matchingDict = IORegistryEntryIDMatching([device registryID]);
            if (matchingDict == nullptr) {
                return DAWN_CONTEXT_LOST_ERROR("Failed to create the matching dict for the device");
            }

            // IOServiceGetMatchingService will consume the reference on the matching dictionary,
            // so we don't need to release the dictionary.
            io_registry_entry_t acceleratorEntry = IOServiceGetMatchingService(kIOMasterPortDefault, matchingDict);
            if (acceleratorEntry == IO_OBJECT_NULL) {
                return DAWN_CONTEXT_LOST_ERROR("Failed to get the IO registry entry for the accelerator");
            }

            // Get the parent entry that will be the IOPCIDevice
            io_registry_entry_t deviceEntry = IO_OBJECT_NULL;
            if (IORegistryEntryGetParentEntry(acceleratorEntry, kIOServicePlane, &deviceEntry) != kIOReturnSuccess) {
                IOObjectRelease(acceleratorEntry);
                return DAWN_CONTEXT_LOST_ERROR("Failed to get the IO registry entry for the device");
            }

            ASSERT(deviceEntry != IO_OBJECT_NULL);
            IOObjectRelease(acceleratorEntry);

            // Get the dictionary of properties for the IOPCIDevice
            CFMutableDictionaryRef deviceProperties;
            if (IORegistryEntryCreateCFProperties(deviceEntry,
                                                      &deviceProperties,
                                                      kCFAllocatorDefault,
                                                      kNilOptions) != kIOReturnSuccess) {
                IOObjectRelease(deviceEntry);
                return DAWN_CONTEXT_LOST_ERROR("Failed to get the IO registry properties for the device");
            }

            ASSERT(deviceProperties != nullptr);
            IOObjectRelease(deviceEntry);

            // Extract the device and vendor ID from the properties. The refs don't need to be
            // released because they follow the "Get" rule.
            CFDataRef vendorIdRef, deviceIdRef;
            Boolean success;
            success = CFDictionaryGetValueIfPresent(deviceProperties, CFSTR("vendor-id"),
                                                   (const void**)&vendorIdRef);
            success &= CFDictionaryGetValueIfPresent(deviceProperties, CFSTR("device-id"),
                                                     (const void**)&deviceIdRef);

            if (!success) {
                CFRelease(deviceProperties);
                return DAWN_CONTEXT_LOST_ERROR("Failed to extract vendor and device ID.");
            }

            uint32_t vendorId = *reinterpret_cast<const uint32_t*>(CFDataGetBytePtr(vendorIdRef));
            uint32_t deviceId = *reinterpret_cast<const uint32_t*>(CFDataGetBytePtr(deviceIdRef));

            CFRelease(deviceProperties);
            return PCIIDs{vendorId, deviceId};
        }
    }  // anonymous namespace

    BackendConnection* Connect(InstanceBase* instance) {
        return nullptr;
    }

    // Device

    Device::Device()
        : DeviceBase(nullptr),
          mMtlDevice(MTLCreateSystemDefaultDevice()),
          mMapTracker(new MapRequestTracker(this)),
          mResourceUploader(new ResourceUploader(this)) {
        [mMtlDevice retain];
        mCommandQueue = [mMtlDevice newCommandQueue];

        if (ConsumedError(CollectPCIInfo())) {
            ASSERT(false);
        }
    }

    Device::~Device() {
        // Wait for all commands to be finished so we can free resources SubmitPendingCommandBuffer
        // may not increment the pendingCommandSerial if there are no pending commands, so we can't
        // store the pendingSerial before SubmitPendingCommandBuffer then wait for it to be passed.
        // Instead we submit and wait for the serial before the next pendingCommandSerial.
        SubmitPendingCommandBuffer();
        while (mCompletedSerial != mLastSubmittedSerial) {
            usleep(100);
        }
        Tick();

        [mPendingCommands release];
        mPendingCommands = nil;

        mMapTracker = nullptr;
        mResourceUploader = nullptr;

        [mMtlDevice release];
        mMtlDevice = nil;

        [mCommandQueue release];
        mCommandQueue = nil;
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
        return mCompletedSerial;
    }

    Serial Device::GetLastSubmittedCommandSerial() const {
        return mLastSubmittedSerial;
    }

    Serial Device::GetPendingCommandSerial() const {
        return mLastSubmittedSerial + 1;
    }

    void Device::TickImpl() {
        mResourceUploader->Tick(mCompletedSerial);
        mMapTracker->Tick(mCompletedSerial);

        if (mPendingCommands != nil) {
            SubmitPendingCommandBuffer();
        } else if (mCompletedSerial == mLastSubmittedSerial) {
            // If there's no GPU work in flight we still need to artificially increment the serial
            // so that CPU operations waiting on GPU completion can know they don't have to wait.
            mCompletedSerial++;
            mLastSubmittedSerial++;
        }
    }

    const dawn_native::PCIInfo& Device::GetPCIInfo() const {
        return mPCIInfo;
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

        // Ok, ObjC blocks are weird. My understanding is that local variables are captured by value
        // so this-> works as expected. However it is unclear how members are captured, (are they
        // captured using this-> or by value?) so we make a copy of the pendingCommandSerial on the
        // stack.
        mLastSubmittedSerial++;
        Serial pendingSerial = mLastSubmittedSerial;
        [mPendingCommands addCompletedHandler:^(id<MTLCommandBuffer>) {
            this->mCompletedSerial = pendingSerial;
        }];

        [mPendingCommands commit];
        [mPendingCommands release];
        mPendingCommands = nil;
    }

    MapRequestTracker* Device::GetMapTracker() const {
        return mMapTracker.get();
    }

    ResourceUploader* Device::GetResourceUploader() const {
        return mResourceUploader.get();
    }

    MaybeError Device::CollectPCIInfo() {
        mPCIInfo.name = std::string([mMtlDevice.name UTF8String]);

        PCIIDs ids;
        DAWN_TRY_ASSIGN(ids, GetDeviceIORegistryPCIInfo(mMtlDevice));
        mPCIInfo.vendorId = ids.vendorId;
        mPCIInfo.deviceId = ids.deviceId;

        return {};
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

}}  // namespace dawn_native::metal
