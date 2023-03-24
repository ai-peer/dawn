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

#include "dawn/native/d3d11/BufferD3D11.h"

#include <algorithm>

#include "dawn/common/Assert.h"
#include "dawn/common/Constants.h"
#include "dawn/common/Math.h"
#include "dawn/native/CommandBuffer.h"
#include "dawn/native/DynamicUploader.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d11/CommandRecordingContextD3D11.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native::d3d11 {

namespace {

D3D11_USAGE D3D11BufferUsage(wgpu::BufferUsage usage) {
    if (usage & wgpu::BufferUsage::MapRead) {
        return D3D11_USAGE_STAGING;
    } else if (usage & wgpu::BufferUsage::MapWrite) {
        return D3D11_USAGE_DYNAMIC;
    } else {
        return D3D11_USAGE_DEFAULT;
    }
}

UINT D3D11BufferBindFlags(wgpu::BufferUsage usage) {
    UINT bindFlags = 0;

    if (usage & (wgpu::BufferUsage::Vertex)) {
        bindFlags |= D3D11_BIND_FLAG::D3D11_BIND_VERTEX_BUFFER;
    }
    if (usage & wgpu::BufferUsage::Index) {
        bindFlags |= D3D11_BIND_FLAG::D3D11_BIND_INDEX_BUFFER;
    }
    if (usage & (wgpu::BufferUsage::Uniform)) {
        bindFlags |= D3D11_BIND_FLAG::D3D11_BIND_CONSTANT_BUFFER;
    }
    if (usage & (wgpu::BufferUsage::Storage | kInternalStorageBuffer)) {
        bindFlags |= D3D11_BIND_FLAG::D3D11_BIND_UNORDERED_ACCESS;
    }
    if (usage & kReadOnlyStorageBuffer) {
        bindFlags |= D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE;
    }

    return bindFlags;
}

UINT D3D11BufferCPUAccessFlags(wgpu::BufferUsage usage) {
    UINT cpuAccessFlags = 0;
    return cpuAccessFlags;
}

UINT D3D11BufferMiscFlags(wgpu::BufferUsage usage) {
    UINT miscFlags = 0;

    if (usage & wgpu::BufferUsage::Storage) {
        miscFlags |= D3D11_RESOURCE_MISC_FLAG::D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    }

    return miscFlags;
}

UINT D3D11BufferStructureByteStride(wgpu::BufferUsage usage) {
    if (usage & (wgpu::BufferUsage::Storage | kInternalStorageBuffer)) {
        return sizeof(uint32_t);
    }
    return 0;
}

size_t D3D11BufferSizeAlignment(wgpu::BufferUsage usage) {
    if (usage & wgpu::BufferUsage::Uniform) {
        return 16;
    }
    if (usage & (wgpu::BufferUsage::Storage | kInternalStorageBuffer)) {
        return sizeof(uint32_t);
    }
    return 1;
}

bool IsStagingUsage(wgpu::BufferUsage usage) {
    constexpr wgpu::BufferUsage kGpuShaderUsages =
        wgpu::BufferUsage::Vertex | wgpu::BufferUsage::Index | wgpu::BufferUsage::Uniform |
        wgpu::BufferUsage::Storage;
    return (usage & kGpuShaderUsages) == 0;
}

}  // namespace

// static
ResultOrError<Ref<Buffer>> Buffer::Create(Device* device, const BufferDescriptor* descriptor) {
    Ref<Buffer> buffer = AcquireRef(new Buffer(device, descriptor));
    DAWN_TRY(buffer->Initialize(descriptor->mappedAtCreation));
    return buffer;
}

Buffer::Buffer(Device* device, const BufferDescriptor* descriptor)
    : BufferBase(device, descriptor) {}

MaybeError Buffer::Initialize(bool mappedAtCreation) {
    // Allocate at least 4 bytes so clamped accesses are always in bounds.
    uint64_t size = std::max(GetSize(), uint64_t(4u));
    size_t alignment = D3D11BufferSizeAlignment(GetUsage());
    if (size > std::numeric_limits<uint64_t>::max() - alignment) {
        // Alignment would overlow.
        return DAWN_OUT_OF_MEMORY_ERROR("Buffer allocation is too large");
    }
    mAllocatedSize = Align(size, alignment);

    const bool isStaging = IsStagingUsage(GetUsage());
    if (!mappedAtCreation && !isStaging) {
        // Create mD3d11Buffer
        D3D11_BUFFER_DESC bufferDescriptor;
        bufferDescriptor.ByteWidth = mAllocatedSize;
        bufferDescriptor.Usage = D3D11BufferUsage(GetUsage());
        bufferDescriptor.BindFlags = D3D11BufferBindFlags(GetUsage());
        bufferDescriptor.CPUAccessFlags = D3D11BufferCPUAccessFlags(GetUsage());
        bufferDescriptor.MiscFlags = D3D11BufferMiscFlags(GetUsage());
        bufferDescriptor.StructureByteStride = D3D11BufferStructureByteStride(GetUsage());

        DAWN_TRY(CheckHRESULT(ToBackend(GetDevice())
                                  ->GetD3D11Device()
                                  ->CreateBuffer(&bufferDescriptor, nullptr, &mD3d11Buffer),
                              "ID3D11Device::CreateBuffer"));
    } else {
        // Create mStagingBuffer, which is used for both staging and mappedAtCreation.
        mStagingBuffer.reset(reinterpret_cast<uint8_t*>(malloc(GetAllocatedSize())));
        if (!mStagingBuffer) {
            return DAWN_OUT_OF_MEMORY_ERROR("Buffer allocation failed");
        }
    }

    SetLabelImpl();
    return {};
}

Buffer::~Buffer() = default;

// // When true is returned, a D3D11_RESOURCE_BARRIER has been created and must be used in a
// // ResourceBarrier call. Failing to do so will cause the tracked state to become invalid and can
// // cause subsequent errors.
// bool Buffer::TrackUsageAndGetResourceBarrier(CommandRecordingContext* commandContext,
//                                              D3D11_RESOURCE_BARRIER* barrier,
//                                              wgpu::BufferUsage newUsage) {
//     // Track the underlying heap to ensure residency.
//     Heap* heap = ToBackend(mResourceAllocation.GetResourceHeap());
//     commandContext->TrackHeapUsage(heap, GetDevice()->GetPendingCommandSerial());

//     MarkUsedInPendingCommands();

//     // Resources in upload and readback heaps must be kept in the COPY_SOURCE/DEST state
//     if (mFixedResourceState) {
//         ASSERT(mLastUsage == newUsage);
//         return false;
//     }

//     D3D11_RESOURCE_STATES lastState = D3D11BufferUsage(mLastUsage);
//     D3D11_RESOURCE_STATES newState = D3D11BufferUsage(newUsage);

//     // If the transition is from-UAV-to-UAV, then a UAV barrier is needed.
//     // If one of the usages isn't UAV, then other barriers are used.
//     bool needsUAVBarrier = lastState == D3D11_RESOURCE_STATE_UNORDERED_ACCESS &&
//                            newState == D3D11_RESOURCE_STATE_UNORDERED_ACCESS;

//     if (needsUAVBarrier) {
//         barrier->Type = D3D11_RESOURCE_BARRIER_TYPE_UAV;
//         barrier->Flags = D3D11_RESOURCE_BARRIER_FLAG_NONE;
//         barrier->UAV.pResource = GetD3D11Resource();

//         mLastUsage = newUsage;
//         return true;
//     }

//     // We can skip transitions to already current usages.
//     if (IsSubset(newUsage, mLastUsage)) {
//         return false;
//     }

//     mLastUsage = newUsage;

//     // The COMMON state represents a state where no write operations can be pending, which makes
//     // it possible to transition to and from some states without synchronizaton (i.e. without an
//     // explicit ResourceBarrier call). A buffer can be implicitly promoted to 1) a single write
//     // state, or 2) multiple read states. A buffer that is accessed within a command list will
//     // always implicitly decay to the COMMON state after the call to ExecuteCommandLists
//     // completes - this is because all buffer writes are guaranteed to be completed before the
//     // next ExecuteCommandLists call executes.
//     //
//     https://docs.microsoft.com/en-us/windows/desktop/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#implicit-state-transitions

//     // To track implicit decays, we must record the pending serial on which a transition will
//     // occur. When that buffer is used again, the previously recorded serial must be compared to
//     // the last completed serial to determine if the buffer has implicity decayed to the common
//     // state.
//     const ExecutionSerial pendingCommandSerial =
//     ToBackend(GetDevice())->GetPendingCommandSerial(); if (pendingCommandSerial >
//     mLastUsedSerial) {
//         lastState = D3D11_RESOURCE_STATE_COMMON;
//         mLastUsedSerial = pendingCommandSerial;
//     }

//     // All possible buffer states used by Dawn are eligible for implicit promotion from COMMON.
//     // These are: COPY_SOURCE, VERTEX_AND_COPY_BUFFER, INDEX_BUFFER, COPY_DEST,
//     // UNORDERED_ACCESS, and INDIRECT_ARGUMENT. Note that for implicit promotion, the
//     // destination state cannot be 1) more than one write state, or 2) both a read and write
//     // state. This goes unchecked here because it should not be allowed through render/compute
//     // pass validation.
//     if (lastState == D3D11_RESOURCE_STATE_COMMON) {
//         return false;
//     }

//     // TODO(crbug.com/dawn/1024): The before and after states must be different. Remove this
//     // workaround and use D3D11 states instead of WebGPU usages to manage the tracking of
//     // barrier state.
//     if (lastState == newState) {
//         return false;
//     }

//     barrier->Type = D3D11_RESOURCE_BARRIER_TYPE_TRANSITION;
//     barrier->Flags = D3D11_RESOURCE_BARRIER_FLAG_NONE;
//     barrier->Transition.pResource = GetD3D11Resource();
//     barrier->Transition.StateBefore = lastState;
//     barrier->Transition.StateAfter = newState;
//     barrier->Transition.Subresource = D3D11_RESOURCE_BARRIER_ALL_SUBRESOURCES;

//     return true;
// }

// void Buffer::TrackUsageAndTransitionNow(CommandRecordingContext* commandContext,
//                                         wgpu::BufferUsage newUsage) {
//     D3D11_RESOURCE_BARRIER barrier;

//     if (TrackUsageAndGetResourceBarrier(commandContext, &barrier, newUsage)) {
//         commandContext->GetCommandList()->ResourceBarrier(1, &barrier);
//     }
// }

// D3D11_GPU_VIRTUAL_ADDRESS Buffer::GetVA() const {
//     return mResourceAllocation.GetGPUPointer();
// }

bool Buffer::IsCPUWritableAtCreation() const {
    // All buffers can be initialized with data at creation, and we will allocate a staging buffer
    // in system memory for it.
    return true;
}

MaybeError Buffer::MapInternal(bool isWrite, size_t offset, size_t size, const char* contextInfo) {
    ASSERT(!mMappedData);
    ASSERT(!mD3d11Buffer);

    mMappedData = mStagingBuffer.get();
    return {};
}

MaybeError Buffer::MapAtCreationImpl() {
    ASSERT(!mD3d11Buffer);

    // The buffers with mappedAtCreation == true will be initialized in
    // BufferBase::MapAtCreation().
    DAWN_TRY(MapInternal(true, 0, size_t(GetAllocatedSize()), "D3D11 map at creation"));

    return {};
}

MaybeError Buffer::MapAsyncImpl(wgpu::MapMode mode, size_t offset, size_t size) {
    ASSERT(IsStagingUsage(GetUsage()));

    DAWN_TRY(EnsureDataInitialized(nullptr));

    DAWN_TRY(MapInternal(mode & wgpu::MapMode::Write, offset, size, "D3D11 map async"));

    return {};
}

void Buffer::UnmapImpl() {
    ASSERT(mMappedData);
    mMappedData = nullptr;

    const bool isStaging = IsStagingUsage(GetUsage());
    if (isStaging) {
        return;
    }

    ASSERT(!mD3d11Buffer);
    // Create mD3d11Buffer and copy the data from mStagingBuffer to mD3d11Buffer.
    D3D11_BUFFER_DESC bufferDescriptor;
    bufferDescriptor.ByteWidth = mAllocatedSize;
    bufferDescriptor.Usage = D3D11BufferUsage(GetUsage());
    bufferDescriptor.BindFlags = D3D11BufferBindFlags(GetUsage());
    bufferDescriptor.CPUAccessFlags = D3D11BufferCPUAccessFlags(GetUsage());
    bufferDescriptor.MiscFlags = D3D11BufferMiscFlags(GetUsage());
    bufferDescriptor.StructureByteStride = D3D11BufferStructureByteStride(GetUsage());

    D3D11_SUBRESOURCE_DATA subresourceData;
    subresourceData.pSysMem = mStagingBuffer.get();
    subresourceData.SysMemPitch = 0;
    subresourceData.SysMemSlicePitch = 0;

    HRESULT hr = ToBackend(GetDevice())
                     ->GetD3D11Device()
                     ->CreateBuffer(&bufferDescriptor, &subresourceData, &mD3d11Buffer);
    ASSERT(SUCCEEDED(hr));
    mStagingBuffer.reset();
}

void* Buffer::GetMappedPointer() {
    // The frontend asks that the pointer returned is from the start of the resource
    // irrespective of the offset passed in MapAsyncImpl, which is what mMappedData is.
    return mMappedData;
}

void Buffer::DestroyImpl() {
    BufferBase::DestroyImpl();
    mD3d11Buffer = nullptr;
}

MaybeError Buffer::EnsureDataInitialized(CommandRecordingContext* commandContext) {
    if (!NeedsInitialization()) {
        return {};
    }

    DAWN_TRY(InitializeToZero(commandContext));
    return {};
}

ResultOrError<bool> Buffer::EnsureDataInitializedAsDestination(
    CommandRecordingContext* commandContext,
    uint64_t offset,
    uint64_t size) {
    if (!NeedsInitialization()) {
        return {false};
    }

    if (IsFullBufferRange(offset, size)) {
        SetIsDataInitialized();
        return {false};
    }

    DAWN_TRY(InitializeToZero(commandContext));
    return {true};
}

MaybeError Buffer::EnsureDataInitializedAsDestination(CommandRecordingContext* commandContext,
                                                      const CopyTextureToBufferCmd* copy) {
    if (!NeedsInitialization()) {
        return {};
    }

    if (IsFullBufferOverwrittenInTextureToBufferCopy(copy)) {
        SetIsDataInitialized();
    } else {
        DAWN_TRY(InitializeToZero(commandContext));
    }

    return {};
}

void Buffer::SetLabelImpl() {
    if (mD3d11Buffer) {
        mD3d11Buffer->SetPrivateData(WKPDID_D3DDebugObjectName, GetLabel().size(),
                                     GetLabel().c_str());
    }
}

MaybeError Buffer::InitializeToZero(CommandRecordingContext* commandContext) {
    ASSERT(NeedsInitialization());

    // TODO(crbug.com/dawn/484): skip initializing the buffer when it is created on a heap
    // that has already been zero initialized.
    DAWN_TRY(ClearBuffer(commandContext, uint8_t(0u)));
    SetIsDataInitialized();
    GetDevice()->IncrementLazyClearCountForTesting();

    return {};
}

MaybeError Buffer::ClearBuffer(CommandRecordingContext* commandContext,
                               uint8_t clearValue,
                               uint64_t offset,
                               uint64_t size) {
    if (size <= 0) {
        DAWN_ASSERT(offset == 0);
        size = GetAllocatedSize();
    }

    if (IsStagingUsage(GetUsage())) {
        memset(mStagingBuffer.get() + offset, clearValue, size);
        return {};
    }

    DAWN_ASSERT(commandContext != nullptr);

    std::vector<uint8_t> clearData(size, clearValue);
    D3D11_BOX dstBox;
    dstBox.left = static_cast<UINT>(offset);
    dstBox.right = static_cast<UINT>(offset + size);
    dstBox.top = 0;
    dstBox.bottom = 1;
    dstBox.front = 0;
    dstBox.back = 1;

    commandContext->GetD3D11DeviceContext1()->UpdateSubresource(mD3d11Buffer.Get(), 0, &dstBox,
                                                                clearData.data(), 0, 0);

    return {};
}
}  // namespace dawn::native::d3d11
