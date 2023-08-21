// Copyright 2017 The Dawn Authors
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

#include "dawn/native/metal/BufferMTL.h"

#include "dawn/common/Math.h"
#include "dawn/common/Platform.h"
#include "dawn/native/CallbackTaskManager.h"
#include "dawn/native/ChainUtils.h"
#include "dawn/native/CommandBuffer.h"
#include "dawn/native/metal/CommandRecordingContext.h"
#include "dawn/native/metal/DeviceMTL.h"
#include "dawn/native/metal/UtilsMetal.h"

#include <limits>

namespace dawn::native::metal {
// The size of uniform buffer and storage buffer need to be aligned to 16 bytes which is the
// largest alignment of supported data types
static constexpr uint32_t kMinUniformOrStorageBufferAlignment = 16u;

// static
ResultOrError<Ref<Buffer>> Buffer::CreateFromHostMappedPointer(
    Device* device,
    const BufferDescriptor* descriptor,
    const BufferHostMappedPointer* hostMappedDesc) {
    if (descriptor->size > std::numeric_limits<NSUInteger>::max()) {
        return DAWN_OUT_OF_MEMORY_ERROR("Buffer allocation is too large");
    }

    Ref<Buffer> buffer = AcquireRef(new Buffer(device, descriptor));

    Ref<Device> deviceRef = device;
    void (*callback)(void*) = hostMappedDesc->disposeCallback;
    void* userdata = hostMappedDesc->userdata;
    auto dispose = ^(void*, NSUInteger) {
        deviceRef->GetCallbackTaskManager()->AddCallbackTask(
            [callback, userdata] { callback(userdata); });
    };

    buffer->mMtlBuffer.Acquire([device->GetMTLDevice()
        newBufferWithBytesNoCopy:hostMappedDesc->pointer
                          length:descriptor->size
                         options:MTLResourceCPUCacheModeDefaultCache
                     deallocator:dispose]);
    if (buffer->mMtlBuffer == nil) {
        dispose(hostMappedDesc->pointer, descriptor->size);
        return DAWN_INTERNAL_ERROR("Buffer allocation failed");
    }
    // Data is assumed to be initialized since it is externally allocated.
    buffer->SetIsDataInitialized();
    buffer->mAllocatedSize = descriptor->size;
    buffer->SetLabelImpl();
    return std::move(buffer);
}

// static
ResultOrError<Ref<Buffer>> Buffer::Create(Device* device, const BufferDescriptor* descriptor) {
    const BufferHostMappedPointer* hostMappedDesc = nullptr;
    FindInChain(descriptor->nextInChain, &hostMappedDesc);
    if (hostMappedDesc != nullptr) {
        return Buffer::CreateFromHostMappedPointer(device, descriptor, hostMappedDesc);
    }

    Ref<Buffer> buffer = AcquireRef(new Buffer(device, descriptor));

    MTLResourceOptions storageMode;
    if (buffer->GetUsage() & kMappableBufferUsages) {
        storageMode = MTLResourceStorageModeShared;
    } else {
        storageMode = MTLResourceStorageModePrivate;
    }

    uint32_t alignment = 1;
#if DAWN_PLATFORM_IS(MACOS)
    // [MTLBlitCommandEncoder fillBuffer] requires the size to be a multiple of 4 on MacOS.
    alignment = 4;
#endif

    // Metal validation layer requires the size of uniform buffer and storage buffer to be no
    // less than the size of the buffer block defined in shader, and the overall size of the
    // buffer must be aligned to the largest alignment of its members.
    if (buffer->GetUsage() &
        (wgpu::BufferUsage::Uniform | wgpu::BufferUsage::Storage | kInternalStorageBuffer)) {
        ASSERT(IsAligned(kMinUniformOrStorageBufferAlignment, alignment));
        alignment = kMinUniformOrStorageBufferAlignment;
    }

    // The vertex pulling transform requires at least 4 bytes in the buffer.
    // 0-sized vertex buffer bindings are allowed, so we always need an additional 4 bytes
    // after the end.
    NSUInteger extraBytes = 0u;
    if ((buffer->GetUsage() & wgpu::BufferUsage::Vertex) != 0) {
        extraBytes = 4u;
    }

    if (buffer->GetSize() > std::numeric_limits<NSUInteger>::max() - extraBytes) {
        return DAWN_OUT_OF_MEMORY_ERROR("Buffer allocation is too large");
    }
    NSUInteger currentSize =
        std::max(static_cast<NSUInteger>(buffer->GetSize()) + extraBytes, NSUInteger(4));

    if (currentSize > std::numeric_limits<NSUInteger>::max() - alignment) {
        // Alignment would overlow.
        return DAWN_OUT_OF_MEMORY_ERROR("Buffer allocation is too large");
    }
    currentSize = Align(currentSize, alignment);

    uint64_t maxBufferSize = QueryMaxBufferLength(device->GetMTLDevice());
    if (currentSize > maxBufferSize) {
        return DAWN_OUT_OF_MEMORY_ERROR("Buffer allocation is too large");
    }

    buffer->mAllocatedSize = currentSize;
    buffer->mMtlBuffer.Acquire([device->GetMTLDevice() newBufferWithLength:currentSize
                                                                   options:storageMode]);
    if (buffer->mMtlBuffer == nullptr) {
        return DAWN_OUT_OF_MEMORY_ERROR("Buffer allocation failed");
    }
    buffer->SetLabelImpl();

    // The buffers with mappedAtCreation == true will be initialized in
    // BufferBase::MapAtCreation().
    if (device->IsToggleEnabled(Toggle::NonzeroClearResourcesOnCreationForTesting) &&
        !descriptor->mappedAtCreation) {
        CommandRecordingContext* commandContext = device->GetPendingCommandContext();
        buffer->ClearBuffer(commandContext, uint8_t(1u));
    }

    // Initialize the padding bytes to zero.
    if (device->IsToggleEnabled(Toggle::LazyClearResourceOnFirstUse) &&
        !descriptor->mappedAtCreation) {
        uint32_t paddingBytes = buffer->GetAllocatedSize() - buffer->GetSize();
        if (paddingBytes > 0) {
            uint32_t clearSize = Align(paddingBytes, 4);
            uint64_t clearOffset = buffer->GetAllocatedSize() - clearSize;

            CommandRecordingContext* commandContext = device->GetPendingCommandContext();
            buffer->ClearBuffer(commandContext, 0, clearOffset, clearSize);
        }
    }

    return std::move(buffer);
}

// static
uint64_t Buffer::QueryMaxBufferLength(id<MTLDevice> mtlDevice) {
    if (@available(iOS 12, tvOS 12, macOS 10.14, *)) {
        return [mtlDevice maxBufferLength];
    }
    // 256Mb limit in versions without based on the data in the feature set tables.
    return 256 * 1024 * 1024;
}

Buffer::Buffer(DeviceBase* dev, const BufferDescriptor* desc) : BufferBase(dev, desc) {}

Buffer::~Buffer() = default;

id<MTLBuffer> Buffer::GetMTLBuffer() const {
    return mMtlBuffer.Get();
}

bool Buffer::IsCPUWritableAtCreation() const {
    // TODO(enga): Handle CPU-visible memory on UMA
    return GetUsage() & kMappableBufferUsages;
}

MaybeError Buffer::MapAtCreationImpl() {
    return {};
}

MaybeError Buffer::MapAsyncImpl(wgpu::MapMode mode, size_t offset, size_t size) {
    CommandRecordingContext* commandContext = ToBackend(GetDevice())->GetPendingCommandContext();
    EnsureDataInitialized(commandContext);

    return {};
}

void* Buffer::GetMappedPointer() {
    return [*mMtlBuffer contents];
}

void Buffer::UnmapImpl() {
    // Nothing to do, Metal StorageModeShared buffers are always mapped.
}

void Buffer::DestroyImpl() {
    BufferBase::DestroyImpl();
    mMtlBuffer = nullptr;
}

void Buffer::TrackUsage() {
    MarkUsedInPendingCommands();
}

bool Buffer::EnsureDataInitialized(CommandRecordingContext* commandContext) {
    if (!NeedsInitialization()) {
        return false;
    }

    InitializeToZero(commandContext);
    return true;
}

bool Buffer::EnsureDataInitializedAsDestination(CommandRecordingContext* commandContext,
                                                uint64_t offset,
                                                uint64_t size) {
    if (!NeedsInitialization()) {
        return false;
    }

    if (IsFullBufferRange(offset, size)) {
        SetIsDataInitialized();
        return false;
    }

    InitializeToZero(commandContext);
    return true;
}

bool Buffer::EnsureDataInitializedAsDestination(CommandRecordingContext* commandContext,
                                                const CopyTextureToBufferCmd* copy) {
    if (!NeedsInitialization()) {
        return false;
    }

    if (IsFullBufferOverwrittenInTextureToBufferCopy(copy)) {
        SetIsDataInitialized();
        return false;
    }

    InitializeToZero(commandContext);
    return true;
}

void Buffer::InitializeToZero(CommandRecordingContext* commandContext) {
    ASSERT(NeedsInitialization());

    ClearBuffer(commandContext, uint8_t(0u));

    SetIsDataInitialized();
    GetDevice()->IncrementLazyClearCountForTesting();
}

void Buffer::ClearBuffer(CommandRecordingContext* commandContext,
                         uint8_t clearValue,
                         uint64_t offset,
                         uint64_t size) {
    ASSERT(commandContext != nullptr);
    size = size > 0 ? size : GetAllocatedSize();
    ASSERT(size > 0);
    TrackUsage();
    [commandContext->EnsureBlit() fillBuffer:mMtlBuffer.Get()
                                       range:NSMakeRange(offset, size)
                                       value:clearValue];
}

void Buffer::SetLabelImpl() {
    SetDebugName(GetDevice(), mMtlBuffer.Get(), "Dawn_Buffer", GetLabel());
}

}  // namespace dawn::native::metal
