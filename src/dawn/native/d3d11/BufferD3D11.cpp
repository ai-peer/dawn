// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "dawn/native/d3d11/BufferD3D11.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "dawn/common/Alloc.h"
#include "dawn/common/Assert.h"
#include "dawn/common/Constants.h"
#include "dawn/common/Math.h"
#include "dawn/native/ChainUtils.h"
#include "dawn/native/CommandBuffer.h"
#include "dawn/native/DynamicUploader.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/QueueD3D11.h"
#include "dawn/native/d3d11/UtilsD3D11.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native::d3d11 {

class ScopedCommandRecordingContext;

namespace {

constexpr wgpu::BufferUsage kD3D11DynamicUniformBufferUsages =
    wgpu::BufferUsage::Uniform | wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc;

constexpr wgpu::BufferUsage kD3D11GPUUniformBufferUsages =
    wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc;

constexpr wgpu::BufferUsage kD3D11UploadBufferUsages =
    wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::MapRead;

constexpr wgpu::BufferUsage kD3D11StagingBufferUsages =
    kD3D11UploadBufferUsages | wgpu::BufferUsage::CopyDst;

constexpr wgpu::BufferUsage kD3D11GPUWriteUsages =
    wgpu::BufferUsage::Storage | kInternalStorageBuffer | wgpu::BufferUsage::Indirect;

// Resource usage    Default    Dynamic   Immutable   Staging
// ------------------------------------------------------------
//  GPU-read         Yes        Yes       Yes         Yes[1]
//  GPU-write        Yes        No        No          Yes[1]
//  CPU-read         No         No        No          Yes[1]
//  CPU-write        No         Yes       No          Yes[1]
// ------------------------------------------------------------
// [1] GPU read or write of a resource with the D3D11_USAGE_STAGING usage is restricted to copy
// operations. You use ID3D11DeviceContext::CopySubresourceRegion and
// ID3D11DeviceContext::CopyResource for these copy operations.

bool IsUpload(wgpu::BufferUsage usage) {
    return IsSubset(usage, kD3D11UploadBufferUsages);
}

bool IsStaging(wgpu::BufferUsage usage) {
    return IsSubset(usage, kD3D11StagingBufferUsages);
}

bool IsPureUniformBuffer(wgpu::BufferUsage usage) {
    return usage & wgpu::BufferUsage::Uniform &&
           (IsSubset(usage, kD3D11DynamicUniformBufferUsages) ||
            IsSubset(usage, kD3D11GPUUniformBufferUsages));
}

UINT D3D11BufferBindFlags(wgpu::BufferUsage usage) {
    UINT bindFlags = 0;

    if (usage & (wgpu::BufferUsage::Vertex)) {
        bindFlags |= D3D11_BIND_VERTEX_BUFFER;
    }
    if (usage & wgpu::BufferUsage::Index) {
        bindFlags |= D3D11_BIND_INDEX_BUFFER;
    }
    if (usage & (wgpu::BufferUsage::Uniform)) {
        bindFlags |= D3D11_BIND_CONSTANT_BUFFER;
    }
    if (usage & (wgpu::BufferUsage::Storage | kInternalStorageBuffer)) {
        bindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }
    if (usage & kReadOnlyStorageBuffer) {
        bindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }

    constexpr wgpu::BufferUsage kCopyUsages =
        wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    // If the buffer only has CopySrc and CopyDst usages are used as staging buffers for copy.
    // Because D3D11 doesn't allow copying between buffer and texture, we will use compute shader
    // to copy data between buffer and texture. So the buffer needs to be bound as unordered access
    // view.
    if (IsSubset(usage, kCopyUsages)) {
        bindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }

    return bindFlags;
}

UINT D3D11BufferMiscFlags(wgpu::BufferUsage usage) {
    UINT miscFlags = 0;
    if (usage & (wgpu::BufferUsage::Storage | kInternalStorageBuffer | kReadOnlyStorageBuffer)) {
        miscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    }
    if (usage & wgpu::BufferUsage::Indirect) {
        miscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    }
    return miscFlags;
}

size_t D3D11BufferSizeAlignment(wgpu::BufferUsage usage) {
    if (usage & wgpu::BufferUsage::Uniform) {
        // https://learn.microsoft.com/en-us/windows/win32/api/d3d11_1/nf-d3d11_1-id3d11devicecontext1-vssetconstantbuffers1
        // Each number of constants must be a multiple of 16 shader constants(sizeof(float) * 4 *
        // 16).
        return sizeof(float) * 4 * 16;
    }

    if (usage & (wgpu::BufferUsage::Storage | kInternalStorageBuffer)) {
        // Unordered access buffers must be 4-byte aligned.
        return sizeof(uint32_t);
    }
    return 1;
}

}  // namespace

// For CPU-to-GPU upload buffers(CopySrc|MapWrite), they can be emulated in the system memory, and
// then written into the dest GPU buffer via ID3D11DeviceContext::UpdateSubresource.
class UploadBuffer final : public Buffer {
    using Buffer::Buffer;
    ~UploadBuffer() override;

    MaybeError InitializeInternal() override;
    MaybeError MapInternal(const ScopedCommandRecordingContext* commandContext,
                           wgpu::MapMode) override;
    void UnmapInternal(const ScopedCommandRecordingContext* commandContext) override;

    MaybeError ClearInternal(const ScopedCommandRecordingContext* commandContext,
                             uint8_t clearValue,
                             uint64_t offset = 0,
                             uint64_t size = 0) override;

    uint8_t* GetUploadData() const override;

    std::unique_ptr<uint8_t[]> mUploadData;
};

class Buffer::Storage : public RefCounted, NonCopyable {
  public:
    Storage(ComPtr<ID3D11Buffer> d3d11Buffer) : mD3d11Buffer(std::move(d3d11Buffer)) {
        D3D11_BUFFER_DESC desc;
        mD3d11Buffer->GetDesc(&desc);
        mD3d11Usage = desc.Usage;
    }

    ID3D11Buffer* GetD3D11Buffer() { return mD3d11Buffer.Get(); }

    uint64_t GetRevision() const { return mRevision; }
    void SetRevision(uint64_t revision) { mRevision = revision; }

    D3D11_USAGE GetD3D11Usage() const { return mD3d11Usage; }

    bool IsCPUWritable() const {
        return mD3d11Usage == D3D11_USAGE_DYNAMIC || mD3d11Usage == D3D11_USAGE_STAGING;
    }
    bool IsCPUReadable() const { return mD3d11Usage == D3D11_USAGE_STAGING; }
    bool IsStaging() const { return IsCPUReadable(); }
    bool IsGPUWritable() const { return mD3d11Usage == D3D11_USAGE_DEFAULT; }

  private:
    ComPtr<ID3D11Buffer> mD3d11Buffer;
    uint64_t mRevision = 0;
    D3D11_USAGE mD3d11Usage;
};

// static
ResultOrError<Ref<Buffer>> Buffer::Create(Device* device,
                                          const UnpackedPtr<BufferDescriptor>& descriptor,
                                          const ScopedCommandRecordingContext* commandContext,
                                          bool allowUploadBufferEmulation) {
    bool useUploadBuffer = allowUploadBufferEmulation;
    useUploadBuffer &= IsUpload(descriptor->usage);
    constexpr uint64_t kMaxUploadBufferSize = 4 * 1024 * 1024;
    useUploadBuffer &= descriptor->size <= kMaxUploadBufferSize;
    Ref<Buffer> buffer;
    if (useUploadBuffer) {
        buffer = AcquireRef(new UploadBuffer(device, descriptor));
    } else {
        buffer = AcquireRef(new Buffer(device, descriptor));
    }
    DAWN_TRY(buffer->Initialize(descriptor->mappedAtCreation, commandContext));
    return buffer;
}

MaybeError Buffer::Initialize(bool mappedAtCreation,
                              const ScopedCommandRecordingContext* commandContext) {
    // TODO(dawn:1705): handle mappedAtCreation for NonzeroClearResourcesOnCreationForTesting

    // Allocate at least 4 bytes so clamped accesses are always in bounds.
    uint64_t size = std::max(GetSize(), uint64_t(4u));
    // The validation layer requires:
    // ByteWidth must be 12 or larger to be used with D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS.
    if (GetUsage() & wgpu::BufferUsage::Indirect) {
        size = std::max(size, uint64_t(12u));
    }
    size_t alignment = D3D11BufferSizeAlignment(GetUsage());
    // Check for overflow, bufferDescriptor.ByteWidth is a UINT.
    if (size > std::numeric_limits<UINT>::max() - alignment) {
        // Alignment would overlow.
        return DAWN_OUT_OF_MEMORY_ERROR("Buffer allocation is too large");
    }
    mAllocatedSize = Align(size, alignment);

    DAWN_TRY(InitializeInternal());

    SetLabelImpl();

    if (!mappedAtCreation) {
        if (GetDevice()->IsToggleEnabled(Toggle::NonzeroClearResourcesOnCreationForTesting)) {
            if (commandContext) {
                DAWN_TRY(ClearInternal(commandContext, 1u));
            } else {
                auto tmpCommandContext =
                    ToBackend(GetDevice()->GetQueue())
                        ->GetScopedPendingCommandContext(QueueBase::SubmitMode::Normal);
                DAWN_TRY(ClearInternal(&tmpCommandContext, 1u));
            }
        }

        // Initialize the padding bytes to zero.
        if (GetDevice()->IsToggleEnabled(Toggle::LazyClearResourceOnFirstUse)) {
            uint32_t paddingBytes = GetAllocatedSize() - GetSize();
            if (paddingBytes > 0) {
                uint32_t clearSize = paddingBytes;
                uint64_t clearOffset = GetSize();
                if (commandContext) {
                    DAWN_TRY(ClearInternal(commandContext, 0, clearOffset, clearSize));

                } else {
                    auto tmpCommandContext =
                        ToBackend(GetDevice()->GetQueue())
                            ->GetScopedPendingCommandContext(QueueBase::SubmitMode::Normal);
                    DAWN_TRY(ClearInternal(&tmpCommandContext, 0, clearOffset, clearSize));
                }
            }
        }
    }

    return {};
}

MaybeError Buffer::InitializeInternal() {
    bool needsConstantBuffer = GetUsage() & wgpu::BufferUsage::Uniform;
    bool onlyNeedsConstantBuffer = IsPureUniformBuffer(GetUsage());

    if (needsConstantBuffer) {
        // Create mConstantBufferStorage
        D3D11_BUFFER_DESC bufferDescriptor;
        bufferDescriptor.ByteWidth = mAllocatedSize;
        if (GetUsage() & wgpu::BufferUsage::MapWrite) {
            bufferDescriptor.Usage = D3D11_USAGE_DYNAMIC;
            bufferDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        } else {
            bufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
            bufferDescriptor.CPUAccessFlags = 0;
        }
        bufferDescriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferDescriptor.MiscFlags = 0;
        bufferDescriptor.StructureByteStride = 0;

        ComPtr<ID3D11Buffer> buffer;
        DAWN_TRY(CheckOutOfMemoryHRESULT(ToBackend(GetDevice())
                                             ->GetD3D11Device()
                                             ->CreateBuffer(&bufferDescriptor, nullptr, &buffer),
                                         "ID3D11Device::CreateBuffer"));

        mConstantBufferStorage = AcquireRef(new Storage(std::move(buffer)));
        mLastUpdatedStorage = mConstantBufferStorage.Get();
        if (mConstantBufferStorage->IsCPUWritable()) {
            mCPUWriteStorage = mConstantBufferStorage;
        }
    }

    if (!onlyNeedsConstantBuffer) {
        // Create mGPUReadStorage
        wgpu::BufferUsage nonUniformUsage = GetUsage() & ~wgpu::BufferUsage::Uniform;
        if (mCPUWriteStorage) {
            // If mConstantBufferStorage already supports MapWrite then exclude it.
            nonUniformUsage = nonUniformUsage & ~wgpu::BufferUsage::MapWrite;
        }
        if (nonUniformUsage & wgpu::BufferUsage::MapWrite) {
            // special case: if a buffer is created with both Storage and MapWrite usages, then we
            // will lazily create a GPU writable storage later. So excluding Storage usage when
            // creating mGPUReadStorage. Note: we favor CPU writable over GPU writable when
            // creating mGPUReadStorage. This is to optimize the most common cases where
            // MapWrite buffers are mostly updated by CPU.
            nonUniformUsage = nonUniformUsage & ~kD3D11GPUWriteUsages;
        }

        if (IsStaging(nonUniformUsage)) {
            DAWN_TRY(AllocateStagingStorageIfNeeded());
            mLastUpdatedStorage = mStagingStorage.Get();
            if (!mCPUWriteStorage) {
                // If mCPUWriteStorage is not already pointing to mConstantBufferStorage then set
                // it here.
                mCPUWriteStorage = mStagingStorage;
            }
        } else {
            D3D11_BUFFER_DESC bufferDescriptor;
            bufferDescriptor.ByteWidth = mAllocatedSize;
            if (nonUniformUsage & wgpu::BufferUsage::MapWrite) {
                bufferDescriptor.Usage = D3D11_USAGE_DYNAMIC;
                bufferDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            } else {
                bufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
                bufferDescriptor.CPUAccessFlags = 0;
            }
            bufferDescriptor.BindFlags = D3D11BufferBindFlags(nonUniformUsage);
            bufferDescriptor.MiscFlags = D3D11BufferMiscFlags(nonUniformUsage);
            bufferDescriptor.StructureByteStride = 0;

            ComPtr<ID3D11Buffer> buffer;
            DAWN_TRY(
                CheckOutOfMemoryHRESULT(ToBackend(GetDevice())
                                            ->GetD3D11Device()
                                            ->CreateBuffer(&bufferDescriptor, nullptr, &buffer),
                                        "ID3D11Device::CreateBuffer"));

            mGPUReadStorage = AcquireRef(new Storage(std::move(buffer)));
            mLastUpdatedStorage = mGPUReadStorage.Get();

            if (mGPUReadStorage->IsCPUWritable()) {
                mCPUWriteStorage = mGPUReadStorage;
            }
            if (mGPUReadStorage->IsGPUWritable()) {
                mGPUWriteStorage = mGPUReadStorage;
            }
        }

        // Special storage for MapRead.
        if (!mStagingStorage && GetUsage() & wgpu::BufferUsage::MapRead) {
            DAWN_TRY(AllocateStagingStorageIfNeeded());
            mLastUpdatedStorage = mStagingStorage.Get();
        }
    }  // if (!onlyNeedsConstantBuffer)

    DAWN_ASSERT(mGPUReadStorage || mConstantBufferStorage || mStagingStorage);

    return {};
}

MaybeError Buffer::AllocateStagingStorageIfNeeded() {
    if (mStagingStorage) {
        return {};
    }
    D3D11_BUFFER_DESC bufferDescriptor;
    bufferDescriptor.ByteWidth = mAllocatedSize;
    bufferDescriptor.Usage = D3D11_USAGE_STAGING;
    // D3D11 doesn't allow copying between buffer and texture.
    //  - For buffer to texture copy, we need to use a staging(mappable) texture, and memcpy
    //    the data from the staging buffer to the staging texture first. So D3D11_CPU_ACCESS_READ
    //    is needed for MapWrite usage.
    //  - For texture to buffer copy, we may need copy texture to a staging (mappable)
    //    texture, and then memcpy the data from the staging texture to the staging buffer.
    //    So D3D11_CPU_ACCESS_WRITE is needed to MapRead usage.
    bufferDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    bufferDescriptor.BindFlags = 0;
    bufferDescriptor.MiscFlags = 0;
    bufferDescriptor.StructureByteStride = 0;

    ComPtr<ID3D11Buffer> buffer;
    DAWN_TRY(CheckOutOfMemoryHRESULT(
        ToBackend(GetDevice())->GetD3D11Device()->CreateBuffer(&bufferDescriptor, nullptr, &buffer),
        "ID3D11Device::CreateBuffer"));

    mStagingStorage = AcquireRef(new Storage(std::move(buffer)));
    return {};
}

MaybeError Buffer::AllocateGPUWriteStorageIfNeeded() {
    if (mGPUWriteStorage) {
        return {};
    }

    // We only care about Storage usage. Because other usages are already covered by
    // mGPUReadStorage. If mGPUReadStorage is also GPU writable then we shouldn't reach this
    // point.
    wgpu::BufferUsage storageOnlyUsage = GetUsage() & kD3D11GPUWriteUsages;
    DAWN_ASSERT(storageOnlyUsage != wgpu::BufferUsage::None);

    D3D11_BUFFER_DESC bufferDescriptor;
    bufferDescriptor.ByteWidth = mAllocatedSize;
    bufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
    bufferDescriptor.CPUAccessFlags = 0;
    bufferDescriptor.BindFlags = D3D11BufferBindFlags(storageOnlyUsage);
    bufferDescriptor.MiscFlags = D3D11BufferMiscFlags(storageOnlyUsage);
    bufferDescriptor.StructureByteStride = 0;

    ComPtr<ID3D11Buffer> buffer;
    DAWN_TRY(CheckOutOfMemoryHRESULT(
        ToBackend(GetDevice())->GetD3D11Device()->CreateBuffer(&bufferDescriptor, nullptr, &buffer),
        "ID3D11Device::CreateBuffer"));

    mGPUWriteStorage = AcquireRef(new Storage(std::move(buffer)));
    return {};
}

Buffer::~Buffer() = default;

bool Buffer::IsCPUWritableAtCreation() const {
    return IsCPUWritable();
}

bool Buffer::IsCPUWritable() const {
    return mCPUWriteStorage != nullptr || GetUploadData() != nullptr;
}

bool Buffer::IsCPUReadable() const {
    return mStagingStorage != nullptr || GetUploadData() != nullptr;
}

MaybeError Buffer::MapInternal(const ScopedCommandRecordingContext* commandContext,
                               wgpu::MapMode mode) {
    DAWN_ASSERT((mode == wgpu::MapMode::Write && IsCPUWritable()) ||
                (mode == wgpu::MapMode::Read && IsCPUReadable()));
    DAWN_ASSERT(!mMappedData);

    D3D11_MAP mapType;
    Storage* storage;
    if (mode == wgpu::MapMode::Write) {
        storage = mCPUWriteStorage.Get();
        mapType = storage->IsStaging() ? D3D11_MAP_READ_WRITE : D3D11_MAP_WRITE_NO_OVERWRITE;
    } else {
        // Always map buffer with D3D11_MAP_READ_WRITE if possible even for mapping
        // wgpu::MapMode:Read, because we need write permission to initialize the buffer.
        // TODO(dawn:1705): investigate the performance impact of mapping with D3D11_MAP_READ_WRITE.
        mapType = D3D11_MAP_READ_WRITE;
        storage = mStagingStorage.Get();
    }

    // Sync previously modified content before mapping.
    DAWN_TRY(SyncStorage(commandContext, storage));

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    DAWN_TRY(CheckHRESULT(commandContext->Map(storage->GetD3D11Buffer(),
                                              /*Subresource=*/0, mapType,
                                              /*MapFlags=*/0, &mappedResource),
                          "ID3D11DeviceContext::Map"));
    mMappedData = reinterpret_cast<uint8_t*>(mappedResource.pData);
    mMappedStorage = storage;

    return {};
}

void Buffer::UnmapInternal(const ScopedCommandRecordingContext* commandContext) {
    DAWN_ASSERT(mMappedData);
    commandContext->Unmap(mMappedStorage->GetD3D11Buffer(),
                          /*Subresource=*/0);
    mMappedData = nullptr;
    // Since D3D11_MAP_READ_WRITE is used even for MapMode::Read, we need to increment the revision.
    IncrementStorageRevisionAndMakeLatest(mMappedStorage);

    if (mStagingStorage && mLastUpdatedStorage != mStagingStorage.Get()) {
        // If we have staging buffer (for MapRead), it has to be updated. The update will run when
        // the command buffer is submitted. Note: This is uncommon case where the buffer is created
        // with both MapRead & MapWrite.
        commandContext->AddBufferForSyncingWithCPU(this);
    }

    mMappedStorage = nullptr;
}

MaybeError Buffer::MapAtCreationImpl() {
    DAWN_ASSERT(IsCPUWritableAtCreation());
    auto commandContext = ToBackend(GetDevice()->GetQueue())
                              ->GetScopedPendingCommandContext(QueueBase::SubmitMode::Normal);
    return MapInternal(&commandContext, wgpu::MapMode::Write);
}

MaybeError Buffer::MapAsyncImpl(wgpu::MapMode mode, size_t offset, size_t size) {
    DAWN_ASSERT((mode == wgpu::MapMode::Write && IsCPUWritable()) ||
                (mode == wgpu::MapMode::Read && IsCPUReadable()));

    mMapReadySerial = mLastUsageSerial;
    const ExecutionSerial completedSerial = GetDevice()->GetQueue()->GetCompletedCommandSerial();
    // We may run into map stall in case that the buffer is still being used by previous submitted
    // commands. To avoid that, instead we ask Queue to do the map later when mLastUsageSerial has
    // passed.
    if (mMapReadySerial > completedSerial) {
        ToBackend(GetDevice()->GetQueue())->TrackPendingMapBuffer({this}, mode, mMapReadySerial);
    } else {
        auto commandContext = ToBackend(GetDevice()->GetQueue())
                                  ->GetScopedPendingCommandContext(QueueBase::SubmitMode::Normal);
        DAWN_TRY(FinalizeMap(&commandContext, completedSerial, mode));
    }

    return {};
}

MaybeError Buffer::FinalizeMap(ScopedCommandRecordingContext* commandContext,
                               ExecutionSerial completedSerial,
                               wgpu::MapMode mode) {
    // Needn't map the buffer if this is for a previous mapAsync that was cancelled.
    if (completedSerial >= mMapReadySerial) {
        // Map then initialize data using mapped pointer.
        // The mapped pointer is always writable because:
        // - If mode is Write, then it's already writable.
        // - If mode is Read, it's only possible to map staging buffer. In that case,
        // D3D11_MAP_READ_WRITE will be used, hence the mapped pointer will also be writable.
        // TODO(dawn:1705): make sure the map call is not blocked by the GPU operations.
        DAWN_TRY(MapInternal(commandContext, mode));
        DAWN_TRY(EnsureDataInitialized(commandContext));
    }

    return {};
}

void Buffer::UnmapImpl() {
    DAWN_ASSERT(mMappedStorage != nullptr || GetUploadData() != nullptr);

    mMapReadySerial = kMaxExecutionSerial;
    if (mMappedData) {
        auto commandContext = ToBackend(GetDevice()->GetQueue())
                                  ->GetScopedPendingCommandContext(QueueBase::SubmitMode::Normal);
        UnmapInternal(&commandContext);
    }
}

void* Buffer::GetMappedPointer() {
    // The frontend asks that the pointer returned is from the start of the resource
    // irrespective of the offset passed in MapAsyncImpl, which is what mMappedData is.
    return mMappedData;
}

void Buffer::DestroyImpl() {
    // TODO(crbug.com/dawn/831): DestroyImpl is called from two places.
    // - It may be called if the buffer is explicitly destroyed with APIDestroy.
    //   This case is NOT thread-safe and needs proper synchronization with other
    //   simultaneous uses of the buffer.
    // - It may be called when the last ref to the buffer is dropped and the buffer
    //   is implicitly destroyed. This case is thread-safe because there are no
    //   other threads using the buffer since there are no other live refs.
    BufferBase::DestroyImpl();
    if (mMappedData) {
        UnmapImpl();
    }
    mConstantBufferStorage = nullptr;
    mCPUWriteStorage = nullptr;
    mStagingStorage = nullptr;
    mGPUWriteStorage = nullptr;
    mGPUReadStorage = nullptr;
    mLastUpdatedStorage = nullptr;
    mMappedStorage = nullptr;
}

void Buffer::SetLabelImpl() {
    if (mGPUReadStorage != nullptr) {
        SetDebugName(ToBackend(GetDevice()), mGPUReadStorage->GetD3D11Buffer(), "Dawn_Buffer",
                     GetLabel());
    }
    if (mConstantBufferStorage != nullptr) {
        SetDebugName(ToBackend(GetDevice()), mConstantBufferStorage->GetD3D11Buffer(),
                     "Dawn_ConstantBuffer", GetLabel());
    }
}

MaybeError Buffer::EnsureDataInitialized(const ScopedCommandRecordingContext* commandContext) {
    if (!NeedsInitialization()) {
        return {};
    }

    DAWN_TRY(InitializeToZero(commandContext));
    return {};
}

MaybeError Buffer::EnsureDataInitializedAsDestination(
    const ScopedCommandRecordingContext* commandContext,
    uint64_t offset,
    uint64_t size) {
    if (!NeedsInitialization()) {
        return {};
    }

    if (IsFullBufferRange(offset, size)) {
        SetInitialized(true);
        return {};
    }

    DAWN_TRY(InitializeToZero(commandContext));
    return {};
}

MaybeError Buffer::EnsureDataInitializedAsDestination(
    const ScopedCommandRecordingContext* commandContext,
    const CopyTextureToBufferCmd* copy) {
    if (!NeedsInitialization()) {
        return {};
    }

    if (IsFullBufferOverwrittenInTextureToBufferCopy(copy)) {
        SetInitialized(true);
    } else {
        DAWN_TRY(InitializeToZero(commandContext));
    }

    return {};
}

MaybeError Buffer::InitializeToZero(const ScopedCommandRecordingContext* commandContext) {
    DAWN_ASSERT(NeedsInitialization());

    DAWN_TRY(ClearInternal(commandContext, uint8_t(0u)));
    SetInitialized(true);
    GetDevice()->IncrementLazyClearCountForTesting();

    return {};
}

MaybeError Buffer::SyncStorage(const ScopedCommandRecordingContext* commandContext,
                               Storage* dstStorage) {
    DAWN_ASSERT(mLastUpdatedStorage);
    DAWN_ASSERT(dstStorage);
    if (mLastUpdatedStorage->GetRevision() == dstStorage->GetRevision()) {
        return {};
    }

    if (dstStorage->IsGPUWritable() || dstStorage->IsStaging()) {
        commandContext->CopyResource(dstStorage->GetD3D11Buffer(),
                                     mLastUpdatedStorage->GetD3D11Buffer());
    } else {
        // TODO(42241146): This is a slow path. It's usually used by uncommon use cases:
        // - GPU writes a CPU writable buffer.
        DAWN_ASSERT(dstStorage->IsCPUWritable());
        DAWN_TRY(AllocateStagingStorageIfNeeded());
        DAWN_TRY(SyncStorage(commandContext, mStagingStorage.Get()));
        D3D11_MAPPED_SUBRESOURCE mappedSrcResource;
        DAWN_TRY(CheckHRESULT(commandContext->Map(mStagingStorage->GetD3D11Buffer(),
                                                  /*Subresource=*/0, D3D11_MAP_READ,
                                                  /*MapFlags=*/0, &mappedSrcResource),
                              "ID3D11DeviceContext::Map src"));

        auto MapAndCopy = [](const ScopedCommandRecordingContext* commandContext, ID3D11Buffer* dst,
                             const void* srcData, size_t size) -> MaybeError {
            D3D11_MAPPED_SUBRESOURCE mappedDstResource;
            DAWN_TRY(CheckHRESULT(commandContext->Map(dst,
                                                      /*Subresource=*/0, D3D11_MAP_WRITE_DISCARD,
                                                      /*MapFlags=*/0, &mappedDstResource),
                                  "ID3D11DeviceContext::Map dst"));
            memcpy(mappedDstResource.pData, srcData, size);
            commandContext->Unmap(dst,
                                  /*Subresource=*/0);
            return {};
        };

        auto result = MapAndCopy(commandContext, dstStorage->GetD3D11Buffer(),
                                 mappedSrcResource.pData, GetSize());

        commandContext->Unmap(mStagingStorage->GetD3D11Buffer(),
                              /*Subresource=*/0);

        if (result.IsError()) {
            return result;
        }
    }

    dstStorage->SetRevision(mLastUpdatedStorage->GetRevision());

    return {};
}

void Buffer::IncrementStorageRevisionAndMakeLatest(Storage* dstStorage) {
    DAWN_ASSERT(dstStorage->GetRevision() == mLastUpdatedStorage->GetRevision());
    dstStorage->SetRevision(dstStorage->GetRevision() + 1);
    mLastUpdatedStorage = dstStorage;
}

MaybeError Buffer::SyncCPUAccessibleStorages(const ScopedCommandRecordingContext* commandContext) {
    if (mConstantBufferStorage && mConstantBufferStorage->IsCPUWritable()) {
        DAWN_TRY(SyncStorage(commandContext, mConstantBufferStorage.Get()));
    }
    if (mGPUReadStorage && mGPUReadStorage->IsCPUWritable()) {
        DAWN_TRY(SyncStorage(commandContext, mGPUReadStorage.Get()));
    }
    if (mStagingStorage) {
        DAWN_TRY(SyncStorage(commandContext, mStagingStorage.Get()));
    }
    return {};
}

ResultOrError<ID3D11Buffer*> Buffer::GetAndSyncD3D11ConstantBuffer(
    const ScopedCommandRecordingContext* commandContext) {
    if (DAWN_UNLIKELY(!mConstantBufferStorage)) {
        return nullptr;
    }
    DAWN_TRY(SyncStorage(commandContext, mConstantBufferStorage.Get()));
    return mConstantBufferStorage->GetD3D11Buffer();
}
ID3D11Buffer* Buffer::GetD3D11ConstantBuffer() {
    DAWN_ASSERT(IsPureUniformBuffer(GetUsage()));
    return mConstantBufferStorage->GetD3D11Buffer();
}

ResultOrError<ID3D11Buffer*> Buffer::GetAndSyncD3D11GPUReadBuffer(
    const ScopedCommandRecordingContext* commandContext) {
    if (DAWN_UNLIKELY(!mGPUReadStorage)) {
        return nullptr;
    }
    DAWN_TRY(SyncStorage(commandContext, mGPUReadStorage.Get()));
    return mGPUReadStorage->GetD3D11Buffer();
}

ResultOrError<ComPtr<ID3D11ShaderResourceView>> Buffer::CreateD3D11ShaderResourceView(
    const ScopedCommandRecordingContext* commandContext,
    uint64_t offset,
    uint64_t size) {
    DAWN_ASSERT(IsAligned(offset, 4u));
    DAWN_ASSERT(IsAligned(size, 4u));

    DAWN_TRY(SyncStorage(commandContext, mGPUReadStorage.Get()));

    UINT firstElement = static_cast<UINT>(offset / 4);
    UINT numElements = static_cast<UINT>(size / 4);

    D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    desc.Format = DXGI_FORMAT_R32_TYPELESS;
    desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    desc.BufferEx.FirstElement = firstElement;
    desc.BufferEx.NumElements = numElements;
    desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    ComPtr<ID3D11ShaderResourceView> srv;
    DAWN_TRY(
        CheckHRESULT(ToBackend(GetDevice())
                         ->GetD3D11Device()
                         ->CreateShaderResourceView(mGPUReadStorage->GetD3D11Buffer(), &desc, &srv),
                     "ShaderResourceView creation"));

    return srv;
}

ResultOrError<ComPtr<ID3D11UnorderedAccessView1>> Buffer::CreateD3D11UnorderedAccessView1(
    const ScopedCommandRecordingContext* commandContext,
    uint64_t offset,
    uint64_t size) {
    DAWN_ASSERT(IsAligned(offset, 4u));
    DAWN_ASSERT(IsAligned(size, 4u));

    DAWN_TRY(AllocateGPUWriteStorageIfNeeded());
    DAWN_TRY(SyncStorage(commandContext, mGPUWriteStorage.Get()));

    UINT firstElement = static_cast<UINT>(offset / 4);
    UINT numElements = static_cast<UINT>(size / 4);

    D3D11_UNORDERED_ACCESS_VIEW_DESC1 desc;
    desc.Format = DXGI_FORMAT_R32_TYPELESS;
    desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = firstElement;
    desc.Buffer.NumElements = numElements;
    desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

    // This buffer is going to be modified in shader. So update its revision and
    // mLastUpdatedStorage.
    ComPtr<ID3D11UnorderedAccessView1> uav;
    DAWN_TRY(CheckHRESULT(
        ToBackend(GetDevice())
            ->GetD3D11Device5()
            ->CreateUnorderedAccessView1(mGPUWriteStorage->GetD3D11Buffer(), &desc, &uav),
        "UnorderedAccessView creation"));

    IncrementStorageRevisionAndMakeLatest(mGPUWriteStorage.Get());

    if (GetUsage() & kMappableBufferUsages) {
        // If this buffer is mappable, we need to copy the content from mGPUWriteStorage to CPU
        // accessible storages.
        commandContext->AddBufferForSyncingWithCPU(this);
    }
    return uav;
}

MaybeError Buffer::Clear(const ScopedCommandRecordingContext* commandContext,
                         uint8_t clearValue,
                         uint64_t offset,
                         uint64_t size) {
    DAWN_ASSERT(!mMappedData);

    if (size == 0) {
        return {};
    }

    // Map the buffer if it is possible, so EnsureDataInitializedAsDestination() and ClearInternal()
    // can write the mapped memory directly.
    ScopedMap scopedMap;
    DAWN_TRY_ASSIGN(scopedMap, ScopedMap::Create(commandContext, this, wgpu::MapMode::Write));

    // For non-staging buffers, we can use UpdateSubresource to write the data.
    DAWN_TRY(EnsureDataInitializedAsDestination(commandContext, offset, size));
    return ClearInternal(commandContext, clearValue, offset, size);
}

MaybeError Buffer::ClearInternal(const ScopedCommandRecordingContext* commandContext,
                                 uint8_t clearValue,
                                 uint64_t offset,
                                 uint64_t size) {
    if (size <= 0) {
        DAWN_ASSERT(offset == 0);
        size = GetAllocatedSize();
    }

    if (mMappedData && IsCPUWritable()) {
        memset(mMappedData.get() + offset, clearValue, size);
        return {};
    }

    // TODO(dawn:1705): use a reusable zero staging buffer to clear the buffer to avoid this CPU to
    // GPU copy.
    std::vector<uint8_t> clearData(size, clearValue);
    return WriteInternal(commandContext, offset, clearData.data(), size);
}

MaybeError Buffer::Write(const ScopedCommandRecordingContext* commandContext,
                         uint64_t offset,
                         const void* data,
                         size_t size) {
    DAWN_ASSERT(size != 0);

    MarkUsedInPendingCommands();
    // Map the buffer if it is possible, so EnsureDataInitializedAsDestination() and WriteInternal()
    // can write the mapped memory directly.
    ScopedMap scopedMap;
    DAWN_TRY_ASSIGN(scopedMap, ScopedMap::Create(commandContext, this, wgpu::MapMode::Write));

    // For non-staging buffers, we can use UpdateSubresource to write the data.
    DAWN_TRY(EnsureDataInitializedAsDestination(commandContext, offset, size));
    return WriteInternal(commandContext, offset, data, size);
}

MaybeError Buffer::WriteInternal(const ScopedCommandRecordingContext* commandContext,
                                 uint64_t offset,
                                 const void* data,
                                 size_t size) {
    if (size == 0) {
        return {};
    }

    // Map the buffer if it is possible, so WriteInternal() can write the mapped memory directly.
    ScopedMap scopedMap;
    DAWN_TRY_ASSIGN(scopedMap, ScopedMap::Create(commandContext, this, wgpu::MapMode::Write));

    if (scopedMap.GetMappedData()) {
        memcpy(scopedMap.GetMappedData() + offset, data, size);
        return {};
    }

    // UpdateSubresource can only be used to update non-mappable buffers.
    DAWN_ASSERT(!IsCPUWritable());

    // WriteInternal() can be called with GetAllocatedSize(). We treat it as a full buffer write as
    // well.
    bool fullSizeWrite = size >= GetSize() && offset == 0;
    if (mGPUReadStorage) {
        D3D11_BOX box;
        box.left = static_cast<UINT>(offset);
        box.top = 0;
        box.front = 0;
        box.right = static_cast<UINT>(offset + size);
        box.bottom = 1;
        box.back = 1;
        if (!fullSizeWrite) {
            DAWN_TRY(SyncStorage(commandContext, mGPUReadStorage.Get()));
        }
        commandContext->UpdateSubresource1(mGPUReadStorage->GetD3D11Buffer(),
                                           /*DstSubresource=*/0,
                                           /*pDstBox=*/&box, data,
                                           /*SrcRowPitch=*/0,
                                           /*SrcDepthPitch=*/0,
                                           /*CopyFlags=*/0);

        IncrementStorageRevisionAndMakeLatest(mGPUReadStorage.Get());

        // No need to update constant buffer at this point, when command buffer wants to bind the
        // constant buffer in a render/compute pass, it will call GetAndSyncD3D11ConstantBuffer()
        // and the constant buffer will be sync-ed there. WriteBuffer() cannot be called inside
        // render/compute pass so no need to sync here.
        return {};
    }

    DAWN_ASSERT(mConstantBufferStorage);

    // For a full size write, UpdateSubresource1(D3D11_COPY_DISCARD) can be used to update
    // mConstantBufferStorage.
    if (fullSizeWrite) {
        // Offset and size must be aligned with 16 for using UpdateSubresource1() on constant
        // buffer.
        constexpr size_t kConstantBufferUpdateAlignment = 16;
        size_t alignedSize = Align(size, kConstantBufferUpdateAlignment);
        DAWN_ASSERT(alignedSize <= GetAllocatedSize());
        std::unique_ptr<uint8_t[]> alignedBuffer;
        if (size != alignedSize) {
            alignedBuffer.reset(new uint8_t[alignedSize]);
            std::memcpy(alignedBuffer.get(), data, size);
            data = alignedBuffer.get();
        }

        D3D11_BOX dstBox;
        dstBox.left = 0;
        dstBox.top = 0;
        dstBox.front = 0;
        dstBox.right = static_cast<UINT>(alignedSize);
        dstBox.bottom = 1;
        dstBox.back = 1;
        // For full buffer write, D3D11_COPY_DISCARD is used to avoid GPU CPU synchronization.
        commandContext->UpdateSubresource1(mConstantBufferStorage->GetD3D11Buffer(),
                                           /*DstSubresource=*/0, &dstBox, data,
                                           /*SrcRowPitch=*/0,
                                           /*SrcDepthPitch=*/0,
                                           /*CopyFlags=*/D3D11_COPY_DISCARD);
        IncrementStorageRevisionAndMakeLatest(mConstantBufferStorage.Get());
        return {};
    }

    // If the mD3d11NonConstantBuffer is null and copy offset and size are not 16 bytes
    // aligned, we have to create a staging buffer for transfer the data to
    // mConstantBufferStorage.
    Ref<BufferBase> stagingBuffer;
    DAWN_TRY_ASSIGN(stagingBuffer, ToBackend(GetDevice())->GetStagingBuffer(commandContext, size));
    stagingBuffer->MarkUsedInPendingCommands();
    DAWN_TRY(ToBackend(stagingBuffer)->WriteInternal(commandContext, 0, data, size));
    DAWN_TRY(Buffer::CopyInternal(commandContext, ToBackend(stagingBuffer.Get()),
                                  /*sourceOffset=*/0,
                                  /*size=*/size, this, offset));
    ToBackend(GetDevice())->ReturnStagingBuffer(std::move(stagingBuffer));

    return {};
}

// static
MaybeError Buffer::Copy(const ScopedCommandRecordingContext* commandContext,
                        Buffer* source,
                        uint64_t sourceOffset,
                        size_t size,
                        Buffer* destination,
                        uint64_t destinationOffset) {
    DAWN_ASSERT(size != 0);

    DAWN_TRY(source->EnsureDataInitialized(commandContext));
    DAWN_TRY(
        destination->EnsureDataInitializedAsDestination(commandContext, destinationOffset, size));
    return CopyInternal(commandContext, source, sourceOffset, size, destination, destinationOffset);
}

// static
MaybeError Buffer::CopyInternal(const ScopedCommandRecordingContext* commandContext,
                                Buffer* source,
                                uint64_t sourceOffset,
                                size_t size,
                                Buffer* destination,
                                uint64_t destinationOffset) {
    // Upload buffers shouldn't be copied to.
    DAWN_ASSERT(!destination->GetUploadData());
    // Use UpdateSubresource1() if the source is an upload buffer.
    if (source->GetUploadData()) {
        DAWN_TRY(destination->WriteInternal(commandContext, destinationOffset,
                                            source->GetUploadData() + sourceOffset, size));
        return {};
    }

    D3D11_BOX srcBox;
    srcBox.left = static_cast<UINT>(sourceOffset);
    srcBox.top = 0;
    srcBox.front = 0;
    srcBox.right = static_cast<UINT>(sourceOffset + size);
    srcBox.bottom = 1;
    srcBox.back = 1;
    Storage* srcStorage = nullptr;
    Storage* dstStorage = nullptr;

    srcStorage = source->GetCopySrcStorage();
    DAWN_TRY_ASSIGN(dstStorage, destination->GetCopyDstStorage());

    DAWN_ASSERT(srcStorage);
    DAWN_ASSERT(dstStorage);

    DAWN_TRY(source->SyncStorage(commandContext, srcStorage));

    if (destinationOffset > 0 || size < destination->GetSize()) {
        DAWN_TRY(destination->SyncStorage(commandContext, dstStorage));
    }

    commandContext->CopySubresourceRegion(dstStorage->GetD3D11Buffer(), /*DstSubresource=*/0,
                                          /*DstX=*/destinationOffset,
                                          /*DstY=*/0,
                                          /*DstZ=*/0, srcStorage->GetD3D11Buffer(),
                                          /*SrcSubresource=*/0, &srcBox);

    destination->IncrementStorageRevisionAndMakeLatest(dstStorage);

    if (destination->GetUsage() & kMappableBufferUsages && !IsStaging(destination->GetUsage())) {
        // If this buffer is mappable, we need to copy the updated content to CPU accessible
        // storages.
        commandContext->AddBufferForSyncingWithCPU(destination);
    }

    return {};
}

Buffer::Storage* Buffer::GetCopySrcStorage() {
    if (mGPUReadStorage) {
        return mGPUReadStorage.Get();
    }
    if (mConstantBufferStorage) {
        return mConstantBufferStorage.Get();
    }
    if (mStagingStorage) {
        return mStagingStorage.Get();
    }

    DAWN_UNREACHABLE();

    return nullptr;
}
ResultOrError<Buffer::Storage*> Buffer::GetCopyDstStorage() {
    if (mGPUReadStorage && mGPUReadStorage->IsGPUWritable()) {
        return mGPUReadStorage.Get();
    }
    if (mConstantBufferStorage && mConstantBufferStorage->IsGPUWritable()) {
        return mConstantBufferStorage.Get();
    }
    if (!mStagingStorage) {
        DAWN_TRY(AllocateStagingStorageIfNeeded());
    }

    return mStagingStorage.Get();
}

uint8_t* Buffer::GetUploadData() const {
    return nullptr;
}

ResultOrError<Buffer::ScopedMap> Buffer::ScopedMap::Create(
    const ScopedCommandRecordingContext* commandContext,
    Buffer* buffer,
    wgpu::MapMode mode) {
    if (mode == wgpu::MapMode::Write && !buffer->IsCPUWritable()) {
        return ScopedMap();
    }
    if (mode == wgpu::MapMode::Read && !buffer->IsCPUReadable()) {
        return ScopedMap();
    }

    if (buffer->mMappedData) {
        return ScopedMap(commandContext, buffer, /*needsUnmap=*/false);
    }

    DAWN_TRY(buffer->MapInternal(commandContext, mode));
    return ScopedMap(commandContext, buffer, /*needsUnmap=*/true);
}

Buffer::ScopedMap::ScopedMap() = default;

Buffer::ScopedMap::ScopedMap(const ScopedCommandRecordingContext* commandContext,
                             Buffer* buffer,
                             bool needsUnmap)
    : mCommandContext(commandContext), mBuffer(buffer), mNeedsUnmap(needsUnmap) {}

Buffer::ScopedMap::~ScopedMap() {
    Reset();
}

Buffer::ScopedMap::ScopedMap(Buffer::ScopedMap&& other) {
    this->operator=(std::move(other));
}

Buffer::ScopedMap& Buffer::ScopedMap::operator=(Buffer::ScopedMap&& other) {
    Reset();
    mCommandContext = other.mCommandContext;
    mBuffer = other.mBuffer;
    mNeedsUnmap = other.mNeedsUnmap;
    other.mBuffer = nullptr;
    other.mNeedsUnmap = false;
    return *this;
}

void Buffer::ScopedMap::Reset() {
    if (mNeedsUnmap) {
        mBuffer->UnmapInternal(mCommandContext);
    }
    mCommandContext = nullptr;
    mBuffer = nullptr;
    mNeedsUnmap = false;
}

uint8_t* Buffer::ScopedMap::GetMappedData() const {
    return mBuffer ? mBuffer->mMappedData.get() : nullptr;
}

UploadBuffer::~UploadBuffer() = default;

MaybeError UploadBuffer::InitializeInternal() {
    mUploadData = std::unique_ptr<uint8_t[]>(AllocNoThrow<uint8_t>(GetAllocatedSize()));
    if (mUploadData == nullptr) {
        return DAWN_OUT_OF_MEMORY_ERROR("Failed to allocate memory for buffer uploading.");
    }
    return {};
}

uint8_t* UploadBuffer::GetUploadData() const {
    return mUploadData.get();
}

MaybeError UploadBuffer::MapInternal(const ScopedCommandRecordingContext* commandContext,
                                     wgpu::MapMode) {
    mMappedData = mUploadData.get();
    return {};
}

void UploadBuffer::UnmapInternal(const ScopedCommandRecordingContext* commandContext) {
    mMappedData = nullptr;
}

MaybeError UploadBuffer::ClearInternal(const ScopedCommandRecordingContext* commandContext,
                                       uint8_t clearValue,
                                       uint64_t offset,
                                       uint64_t size) {
    if (size == 0) {
        DAWN_ASSERT(offset == 0);
        size = GetAllocatedSize();
    }

    memset(mUploadData.get() + offset, clearValue, size);
    return {};
}

}  // namespace dawn::native::d3d11
