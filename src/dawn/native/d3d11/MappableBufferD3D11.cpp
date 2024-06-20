// Copyright 2024 The Dawn & Tint Authors
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

#include "dawn/native/d3d11/MappableBufferD3D11.h"

#include <algorithm>

#include "absl/container/inlined_vector.h"
#include "dawn/native/ChainUtils.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d11/CommandRecordingContextD3D11.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/QueueD3D11.h"
#include "dawn/native/d3d11/UtilsD3D11.h"

namespace dawn::native::d3d11 {

namespace {

constexpr wgpu::BufferUsage kD3D11DynamicUniformBufferUsages =
    wgpu::BufferUsage::Uniform | wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc;

constexpr wgpu::BufferUsage kD3D11GPUWriteUsages =
    wgpu::BufferUsage::Storage | kInternalStorageBuffer | wgpu::BufferUsage::Indirect;

}  // namespace

class MappableBuffer::Storage : public RefCounted, NonCopyable {
  public:
    explicit Storage(ComPtr<ID3D11Buffer> d3d11Buffer) : mD3d11Buffer(std::move(d3d11Buffer)) {
        D3D11_BUFFER_DESC desc;
        mD3d11Buffer->GetDesc(&desc);
        mD3d11Usage = desc.Usage;

        mMappableCopyableFlags = wgpu::BufferUsage::CopySrc;

        switch (mD3d11Usage) {
            case D3D11_USAGE_STAGING:
                mMappableCopyableFlags |= kMappableBufferUsages | wgpu::BufferUsage::CopyDst;
                break;
            case D3D11_USAGE_DYNAMIC:
                mMappableCopyableFlags |= wgpu::BufferUsage::MapWrite;
                break;
            case D3D11_USAGE_DEFAULT:
                mMappableCopyableFlags |= wgpu::BufferUsage::CopyDst;
                break;
            default:
                break;
        }
    }

    ID3D11Buffer* GetD3D11Buffer() { return mD3d11Buffer.Get(); }

    uint64_t GetRevision() const { return mRevision; }
    void SetRevision(uint64_t revision) { mRevision = revision; }
    bool IsFirstRevision() const { return mRevision == 0; }

    bool IsCPUWritable() const { return mMappableCopyableFlags & wgpu::BufferUsage::MapWrite; }
    bool IsCPUReadable() const { return mMappableCopyableFlags & wgpu::BufferUsage::MapRead; }
    bool IsStaging() const { return IsCPUReadable(); }
    bool SupportsCopyDst() const { return mMappableCopyableFlags & wgpu::BufferUsage::CopyDst; }
    bool IsGPUWritable() const { return mD3d11Usage == D3D11_USAGE_DEFAULT; }

  private:
    ComPtr<ID3D11Buffer> mD3d11Buffer;
    uint64_t mRevision = 0;
    D3D11_USAGE mD3d11Usage;
    wgpu::BufferUsage mMappableCopyableFlags;
};

MappableBuffer::MappableBuffer(DeviceBase* device, const UnpackedPtr<BufferDescriptor>& descriptor)
    : GPUUsableBuffer(device,
                      descriptor,
                      /*internalMappableFlags=*/descriptor->usage & kMappableBufferUsages) {}

MappableBuffer::~MappableBuffer() = default;

void MappableBuffer::DestroyImpl() {
    // TODO(crbug.com/dawn/831): DestroyImpl is called from two places.
    // - It may be called if the buffer is explicitly destroyed with APIDestroy.
    //   This case is NOT thread-safe and needs proper synchronization with other
    //   simultaneous uses of the buffer.
    // - It may be called when the last ref to the buffer is dropped and the buffer
    //   is implicitly destroyed. This case is thread-safe because there are no
    //   other threads using the buffer since there are no other live refs.
    GPUUsableBuffer::DestroyImpl();

    mStorages = {};

    mLastUpdatedStorage = nullptr;
    mCPUWritableStorage = nullptr;
    mMappedStorage = nullptr;
}

void MappableBuffer::SetLabelImpl() {
    for (auto ite = mStorages.begin(); ite != mStorages.end(); ++ite) {
        auto storageType = static_cast<StorageType>(std::distance(mStorages.begin(), ite));
        SetStorageLabel(storageType);
    }
}

void MappableBuffer::SetStorageLabel(StorageType storageType) {
    static constexpr ityp::array<MappableBuffer::StorageType, const char*,
                                 static_cast<uint8_t>(StorageType::Count)>
        kStorageTypeStrings = {
            "CPUWritableConstantBuffer",
            "GPUCopyDstConstantBuffer",
            "CPUWritableNonConstantBuffer",
            "GPUWritableNonConstantBuffer",
            "Staging",
        };

    if (!mStorages[storageType]) {
        return;
    }

    SetDebugName(ToBackend(GetDevice()), mStorages[storageType]->GetD3D11Buffer(),
                 kStorageTypeStrings[storageType], GetLabel());
}

MaybeError MappableBuffer::InitializeInternal() {
    DAWN_ASSERT(!IsD3D11BufferUsageStaging(GetUsage()));
    DAWN_ASSERT(GetUsage() & kMappableBufferUsages);

    mStorages = {};

    bool needsConstantBuffer = GetUsage() & wgpu::BufferUsage::Uniform;
    bool onlyNeedsConstantBuffer = IsSubset(GetUsage(), kD3D11DynamicUniformBufferUsages);

    if (needsConstantBuffer) {
        if (GetUsage() & wgpu::BufferUsage::MapWrite) {
            DAWN_TRY_ASSIGN(mLastUpdatedStorage,
                            AllocateStorageIfNeeded(StorageType::CPUWritableConstantBuffer));
            mCPUWritableStorage = mLastUpdatedStorage;
        } else {
            DAWN_TRY_ASSIGN(mLastUpdatedStorage,
                            AllocateStorageIfNeeded(StorageType::GPUCopyDstConstantBuffer));
        }
    }

    if (!onlyNeedsConstantBuffer) {
        // Create non-constant buffer storage.
        wgpu::BufferUsage nonUniformUsage = GetUsage() & ~wgpu::BufferUsage::Uniform;
        if (mCPUWritableStorage) {
            // If CPUWritableConstantBuffer is already present then exclude MapWrite from
            // non-constant buffer storages.
            nonUniformUsage = nonUniformUsage & ~wgpu::BufferUsage::MapWrite;
        }

        if (IsD3D11BufferUsageStaging(nonUniformUsage)) {
            DAWN_TRY_ASSIGN(mLastUpdatedStorage, AllocateStorageIfNeeded(StorageType::Staging));
        } else {
            if (nonUniformUsage & wgpu::BufferUsage::MapWrite) {
                // special case: if a buffer is created with both Storage and MapWrite usages, then
                // we will lazily create a GPU writable storage later. Note: we favor CPU writable
                // over GPU writable when creating non-constant buffer storage. This is to optimize
                // the most common cases where MapWrite buffers are mostly updated by CPU.
                DAWN_TRY_ASSIGN(mLastUpdatedStorage,
                                AllocateStorageIfNeeded(StorageType::CPUWritableNonConstantBuffer));
                mCPUWritableStorage = mLastUpdatedStorage;
            } else {
                DAWN_TRY_ASSIGN(mLastUpdatedStorage,
                                AllocateStorageIfNeeded(StorageType::GPUWritableNonConstantBuffer));
            }

            // Special storage for MapRead.
            if (GetUsage() & wgpu::BufferUsage::MapRead) {
                DAWN_TRY_ASSIGN(mLastUpdatedStorage, AllocateStorageIfNeeded(StorageType::Staging));
            }
        }
    }  // if (!onlyNeedsConstantBuffer)

    DAWN_ASSERT(mLastUpdatedStorage);

    return {};
}

ResultOrError<MappableBuffer::Storage*> MappableBuffer::AllocateStorageIfNeeded(
    StorageType storageType) {
    if (mStorages[storageType]) {
        return mStorages[storageType].Get();
    }
    D3D11_BUFFER_DESC bufferDescriptor;
    bufferDescriptor.ByteWidth = GetAllocatedSize();
    bufferDescriptor.StructureByteStride = 0;

    switch (storageType) {
        case StorageType::CPUWritableConstantBuffer:
            bufferDescriptor.Usage = D3D11_USAGE_DYNAMIC;
            bufferDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            bufferDescriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            bufferDescriptor.MiscFlags = 0;
            break;
        case StorageType::GPUCopyDstConstantBuffer:
            bufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
            bufferDescriptor.CPUAccessFlags = 0;
            bufferDescriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            bufferDescriptor.MiscFlags = 0;
            break;
        case StorageType::CPUWritableNonConstantBuffer: {
            // Need to exclude GPU writable usages because CPU writable buffer is not GPU writable
            // in D3D11.
            auto nonUniformUsage =
                GetUsage() & ~(kD3D11GPUWriteUsages | wgpu::BufferUsage::Uniform);
            bufferDescriptor.Usage = D3D11_USAGE_DYNAMIC;
            bufferDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            bufferDescriptor.BindFlags = D3D11BufferBindFlags(nonUniformUsage);
            bufferDescriptor.MiscFlags = D3D11BufferMiscFlags(nonUniformUsage);
            if (bufferDescriptor.BindFlags == 0) {
                // Dynamic buffer requires at least one binding flag. If no binding flag is needed
                // (one example is MapWrite | QueryResolve), then use D3D11_BIND_INDEX_BUFFER.
                bufferDescriptor.BindFlags = D3D11_BIND_INDEX_BUFFER;
                DAWN_ASSERT(bufferDescriptor.MiscFlags == 0);
            }
        } break;
        case StorageType::GPUWritableNonConstantBuffer: {
            // Need to exclude mapping usages.
            const auto nonUniformUsage =
                GetUsage() & ~(kMappableBufferUsages | wgpu::BufferUsage::Uniform);
            bufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
            bufferDescriptor.CPUAccessFlags = 0;
            bufferDescriptor.BindFlags = D3D11BufferBindFlags(nonUniformUsage);
            bufferDescriptor.MiscFlags = D3D11BufferMiscFlags(nonUniformUsage);
        } break;
        case StorageType::Staging: {
            bufferDescriptor.Usage = D3D11_USAGE_STAGING;
            bufferDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            bufferDescriptor.BindFlags = 0;
            bufferDescriptor.MiscFlags = 0;
        } break;
        case StorageType::Count:
            DAWN_UNREACHABLE();
    }

    ComPtr<ID3D11Buffer> buffer;
    DAWN_TRY(CheckOutOfMemoryHRESULT(
        ToBackend(GetDevice())->GetD3D11Device()->CreateBuffer(&bufferDescriptor, nullptr, &buffer),
        "ID3D11Device::CreateBuffer"));

    mStorages[storageType] = AcquireRef(new Storage(std::move(buffer)));

    SetStorageLabel(storageType);

    return mStorages[storageType].Get();
}

ResultOrError<MappableBuffer::Storage*> MappableBuffer::AllocateDstCopyableStorageIfNeeded() {
    if (mStorages[StorageType::GPUCopyDstConstantBuffer]) {
        return mStorages[StorageType::GPUCopyDstConstantBuffer].Get();
    }
    if (mStorages[StorageType::GPUWritableNonConstantBuffer]) {
        return mStorages[StorageType::GPUWritableNonConstantBuffer].Get();
    }

    if (GetUsage() & wgpu::BufferUsage::Uniform) {
        return AllocateStorageIfNeeded(StorageType::GPUCopyDstConstantBuffer);
    }

    return AllocateStorageIfNeeded(StorageType::GPUWritableNonConstantBuffer);
}

MaybeError MappableBuffer::SyncStorage(const ScopedCommandRecordingContext* commandContext,
                                       Storage* dstStorage) {
    DAWN_ASSERT(mLastUpdatedStorage);
    DAWN_ASSERT(dstStorage);
    if (mLastUpdatedStorage->GetRevision() == dstStorage->GetRevision()) {
        return {};
    }

    if (dstStorage->SupportsCopyDst()) {
        commandContext->CopyResource(dstStorage->GetD3D11Buffer(),
                                     mLastUpdatedStorage->GetD3D11Buffer());
    } else {
        // TODO(42241146): This is a slow path. It's usually used by uncommon use cases:
        // - GPU writes a CPU writable buffer.
        DAWN_ASSERT(dstStorage->IsCPUWritable());
        Storage* stagingStorage;
        DAWN_TRY_ASSIGN(stagingStorage, AllocateStorageIfNeeded(StorageType::Staging));
        DAWN_TRY(SyncStorage(commandContext, stagingStorage));
        D3D11_MAPPED_SUBRESOURCE mappedSrcResource;
        DAWN_TRY(CheckHRESULT(commandContext->Map(stagingStorage->GetD3D11Buffer(),
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
                                 mappedSrcResource.pData, GetAllocatedSize());

        commandContext->Unmap(stagingStorage->GetD3D11Buffer(),
                              /*Subresource=*/0);

        if (result.IsError()) {
            return result;
        }
    }

    dstStorage->SetRevision(mLastUpdatedStorage->GetRevision());

    return {};
}

void MappableBuffer::IncrementStorageRevisionAndMakeLatest(Storage* dstStorage) {
    DAWN_ASSERT(dstStorage->GetRevision() == mLastUpdatedStorage->GetRevision());
    dstStorage->SetRevision(dstStorage->GetRevision() + 1);
    mLastUpdatedStorage = dstStorage;
}

MaybeError MappableBuffer::SyncCPUAccessibleStorages(
    const ScopedCommandRecordingContext* commandContext) {
    Storage* stagingStorage = mStorages[StorageType::Staging].Get();
    if (mCPUWritableStorage && !stagingStorage) {
        // Only sync staging storage. Later other CPU writable storages can be updated by copying
        // from staging storage with Map(MAP_WRITE_DISCARD) which won't stall the CPU. Otherwise,
        // since CPU writable storages don't support CopyDst, it would require a CPU stall in order
        // to sync them here.
        DAWN_TRY_ASSIGN(stagingStorage, AllocateStorageIfNeeded(StorageType::Staging));
    }

    if (stagingStorage) {
        return SyncStorage(commandContext, stagingStorage);
    }

    return {};
}

MaybeError MappableBuffer::MapInternal(const ScopedCommandRecordingContext* commandContext,
                                       wgpu::MapMode mode) {
    DAWN_ASSERT(!mMappedData);

    D3D11_MAP mapType;
    Storage* storage;
    if (mode == wgpu::MapMode::Write) {
        DAWN_ASSERT(!mCPUWritableStorage->IsStaging());
        // Use D3D11_MAP_WRITE_NO_OVERWRITE to guarantee driver that we don't overwrite data in use
        // by GPU. MapAsync() already ensures that any GPU commands using this buffer already
        // finish. In return driver won't try to stall CPU for mapping access.
        mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
        storage = mCPUWritableStorage;
    } else {
        // Always map buffer with D3D11_MAP_READ_WRITE if possible even for mapping
        // wgpu::MapMode:Read, because we need write permission to initialize the buffer.
        // TODO(dawn:1705): investigate the performance impact of mapping with D3D11_MAP_READ_WRITE.
        mapType = D3D11_MAP_READ_WRITE;
        // If buffer has MapRead usage, a staging storage should already be created in
        // InitializeInternal().
        storage = mStorages[StorageType::Staging].Get();
    }

    DAWN_ASSERT(storage);

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

void MappableBuffer::UnmapInternal(const ScopedCommandRecordingContext* commandContext) {
    DAWN_ASSERT(mMappedData);
    commandContext->Unmap(mMappedStorage->GetD3D11Buffer(),
                          /*Subresource=*/0);
    mMappedData = nullptr;
    // Since D3D11_MAP_READ_WRITE is used even for MapMode::Read, we need to increment the revision.
    IncrementStorageRevisionAndMakeLatest(mMappedStorage);

    auto* stagingStorage = mStorages[StorageType::Staging].Get();

    if (stagingStorage && mLastUpdatedStorage != stagingStorage) {
        // If we have staging buffer (for MapRead), it has to be updated. Note: This is uncommon
        // case where the buffer is created with both MapRead & MapWrite. Technically it's
        // impossible for the following code to return error. Because in staging storage case, only
        // CopyResource() needs to be used. No extra allocations needed.
        [[maybe_unused]] bool consumed =
            GetDevice()->ConsumedError(SyncStorage(commandContext, stagingStorage));
    }

    mMappedStorage = nullptr;
}

ResultOrError<ID3D11Buffer*> MappableBuffer::GetD3D11ConstantBuffer(
    const ScopedCommandRecordingContext* commandContext) {
    auto* storage = mStorages[StorageType::CPUWritableConstantBuffer].Get();
    if (storage) {
        if (storage->GetRevision() != mLastUpdatedStorage->GetRevision()) {
            // This could happen if mappable uniform buffer was used as destination of CopyB2B
            // priorly. Since updating this CPU writable constant buffer could require a CPU stall,
            // we should use GPUCopyDstConstantBuffer storage instead. It will work with CopyB2B and
            // won't stall CPU.
            DAWN_TRY_ASSIGN(storage,
                            AllocateStorageIfNeeded(StorageType::GPUCopyDstConstantBuffer));
        }
    } else {
        storage = mStorages[StorageType::GPUCopyDstConstantBuffer].Get();
    }

    DAWN_TRY(SyncStorage(commandContext, storage));
    return storage->GetD3D11Buffer();
}

ResultOrError<ID3D11Buffer*> MappableBuffer::GetD3D11NonConstantBuffer(
    const ScopedCommandRecordingContext* commandContext) {
    auto* storage = mStorages[StorageType::CPUWritableNonConstantBuffer].Get();
    if (storage) {
        if (storage->GetRevision() != mLastUpdatedStorage->GetRevision()) {
            // This could happen if mappable uniform buffer was used on GPU before. Since updating
            // this CPU writable buffer could require a CPU stall. We should use
            // GPUWritableNonConstantBuffer storage instead. It will work with CopyB2B and won't
            // stall CPU.
            DAWN_TRY_ASSIGN(storage,
                            AllocateStorageIfNeeded(StorageType::GPUWritableNonConstantBuffer));
        }
    } else {
        storage = mStorages[StorageType::GPUWritableNonConstantBuffer].Get();
    }

    DAWN_TRY(SyncStorage(commandContext, storage));

    return storage->GetD3D11Buffer();
}

ResultOrError<ComPtr<ID3D11ShaderResourceView>> MappableBuffer::CreateD3D11ShaderResourceView(
    const ScopedCommandRecordingContext* commandContext,
    uint64_t offset,
    uint64_t size) {
    ID3D11Buffer* d3dBuffer;

    DAWN_TRY_ASSIGN(d3dBuffer, GetD3D11NonConstantBuffer(commandContext));

    return CreateD3D11ShaderResourceViewFromD3DBuffer(d3dBuffer, offset, size);
}

ResultOrError<ComPtr<ID3D11UnorderedAccessView1>>
MappableBuffer::CreateD3D11UnorderedAccessViewAndMarkMutatedByShader(
    const ScopedCommandRecordingContext* commandContext,
    uint64_t offset,
    uint64_t size) {
    Storage* storage = nullptr;
    DAWN_TRY_ASSIGN(storage, AllocateStorageIfNeeded(StorageType::GPUWritableNonConstantBuffer));
    DAWN_TRY(SyncStorage(commandContext, storage));

    ComPtr<ID3D11UnorderedAccessView1> uav;
    DAWN_TRY_ASSIGN(
        uav, CreateD3D11UnorderedAccessViewFromD3DBuffer(storage->GetD3D11Buffer(), offset, size));

    // Since UAV will modify the storage's content, increment its revision.
    IncrementStorageRevisionAndMakeLatest(storage);

    if (GetUsage() & kMappableBufferUsages) {
        // If this buffer is mappable, we need to copy the content from GPUWritableNonConstantBuffer
        // storage to CPU accessible storages at the end of the current command buffer.
        commandContext->AddBufferForSyncingWithCPU(this);
    }

    return uav;
}

MaybeError MappableBuffer::WriteInternal(const ScopedCommandRecordingContext* commandContext,
                                         uint64_t offset,
                                         const void* data,
                                         size_t size) {
    if (size == 0) {
        return {};
    }

    // Map the buffer if it is possible, so WriteInternal() can write the mapped memory directly.
    if (IsCPUWritable()) {
        if (mLastUsageSerial <= GetDevice()->GetQueue()->GetCompletedCommandSerial()) {
            ScopedMap scopedMap;
            DAWN_TRY_ASSIGN(scopedMap,
                            ScopedMap::Create(commandContext, this, wgpu::MapMode::Write));

            DAWN_ASSERT(scopedMap.GetMappedData());
            memcpy(scopedMap.GetMappedData() + offset, data, size);
            return {};
        } else {
            // Mapping buffer at this point would stall the CPU. We will create a GPU copyable
            // storage and use UpdateSubresource on it below instead.
            Storage* gpuCopyableStorage;
            DAWN_TRY_ASSIGN(gpuCopyableStorage, AllocateDstCopyableStorageIfNeeded());
            DAWN_TRY(SyncStorage(commandContext, gpuCopyableStorage));
        }
    }

    if (GetUsage() & kMappableBufferUsages) {
        // If this buffer is mappable, we need to update the CPU accessible storages at the end of
        // the current command buffer.
        commandContext->AddBufferForSyncingWithCPU(this);
    }

    // WriteInternal() can be called with GetAllocatedSize(). We treat it as a full buffer write as
    // well.
    bool fullSizeWrite = size >= GetSize() && offset == 0;
    auto* nonConstantStorage = mStorages[StorageType::GPUWritableNonConstantBuffer].Get();
    if (nonConstantStorage) {
        D3D11_BOX box;
        box.left = static_cast<UINT>(offset);
        box.top = 0;
        box.front = 0;
        box.right = static_cast<UINT>(offset + size);
        box.bottom = 1;
        box.back = 1;
        if (!fullSizeWrite) {
            DAWN_TRY(SyncStorage(commandContext, nonConstantStorage));
        }
        commandContext->UpdateSubresource1(nonConstantStorage->GetD3D11Buffer(),
                                           /*DstSubresource=*/0,
                                           /*pDstBox=*/&box, data,
                                           /*SrcRowPitch=*/0,
                                           /*SrcDepthPitch=*/0,
                                           /*CopyFlags=*/0);

        IncrementStorageRevisionAndMakeLatest(nonConstantStorage);

        // No need to update constant buffer at this point, when command buffer wants to bind the
        // constant buffer in a render/compute pass, it will call GetD3D11ConstantBuffer()
        // and the constant buffer will be sync-ed there. WriteBuffer() cannot be called inside
        // render/compute pass so no need to sync here.
        return {};
    }

    auto* constantStorage = mStorages[StorageType::GPUCopyDstConstantBuffer].Get();
    DAWN_ASSERT(constantStorage);

    if (!fullSizeWrite) {
        DAWN_TRY(SyncStorage(commandContext, constantStorage));
    }

    DAWN_TRY(UpdateD3D11ConstantBuffer(commandContext, constantStorage->GetD3D11Buffer(),
                                       /*firstTimeUpdate=*/constantStorage->IsFirstRevision(),
                                       offset, data, size));

    IncrementStorageRevisionAndMakeLatest(constantStorage);

    return {};
}

MaybeError MappableBuffer::CopyToInternal(const ScopedCommandRecordingContext* commandContext,
                                          uint64_t sourceOffset,
                                          size_t size,
                                          Buffer* destination,
                                          uint64_t destinationOffset) {
    ID3D11Buffer* d3d11SourceBuffer = mLastUpdatedStorage->GetD3D11Buffer();

    return destination->CopyFromD3DInternal(commandContext, d3d11SourceBuffer, sourceOffset, size,
                                            destinationOffset);
}

MaybeError MappableBuffer::CopyFromD3DInternal(const ScopedCommandRecordingContext* commandContext,
                                               ID3D11Buffer* d3d11SourceBuffer,
                                               uint64_t sourceOffset,
                                               size_t size,
                                               uint64_t destinationOffset) {
    D3D11_BOX srcBox;
    srcBox.left = static_cast<UINT>(sourceOffset);
    srcBox.top = 0;
    srcBox.front = 0;
    srcBox.right = static_cast<UINT>(sourceOffset + size);
    srcBox.bottom = 1;
    srcBox.back = 1;

    Storage* gpuCopyableStorage;
    DAWN_TRY_ASSIGN(gpuCopyableStorage, AllocateDstCopyableStorageIfNeeded());
    DAWN_TRY(SyncStorage(commandContext, gpuCopyableStorage));

    commandContext->CopySubresourceRegion(
        gpuCopyableStorage->GetD3D11Buffer(), /*DstSubresource=*/0,
        /*DstX=*/destinationOffset,
        /*DstY=*/0,
        /*DstZ=*/0, d3d11SourceBuffer, /*SrcSubresource=*/0, &srcBox);

    IncrementStorageRevisionAndMakeLatest(gpuCopyableStorage);

    if (GetUsage() & kMappableBufferUsages) {
        commandContext->AddBufferForSyncingWithCPU(this);
    }

    return {};
}

MaybeError MappableBuffer::PredicatedClear(
    const ScopedSwapStateCommandRecordingContext* commandContext,
    ID3D11Predicate* predicate,
    uint8_t clearValue,
    uint64_t offset,
    uint64_t size) {
    DAWN_ASSERT(size != 0);

    // Don't use mapping, mapping is not affected by ID3D11Predicate.
    // Allocate GPU writable storage and sync it. Note: we don't SetPredication() yet otherwise it
    // would affect the syncing.
    Storage* gpuWritableStorage;
    DAWN_TRY_ASSIGN(gpuWritableStorage,
                    AllocateStorageIfNeeded(StorageType::GPUWritableNonConstantBuffer));
    DAWN_TRY(SyncStorage(commandContext, gpuWritableStorage));

    // SetPredication() and clear the storage with UpdateSubresource1().
    D3D11_BOX box;
    box.left = static_cast<UINT>(offset);
    box.top = 0;
    box.front = 0;
    box.right = static_cast<UINT>(offset + size);
    box.bottom = 1;
    box.back = 1;

    absl::InlinedVector<uint8_t, sizeof(uint64_t)> clearData(size, clearValue);

    // The update will not be performed if the predicate's data is false.
    commandContext->GetD3D11DeviceContext4()->SetPredication(predicate, false);
    commandContext->UpdateSubresource1(gpuWritableStorage->GetD3D11Buffer(),
                                       /*DstSubresource=*/0,
                                       /*pDstBox=*/&box, clearData.data(),
                                       /*SrcRowPitch=*/0,
                                       /*SrcDepthPitch=*/0,
                                       /*CopyFlags=*/0);
    commandContext->GetD3D11DeviceContext4()->SetPredication(nullptr, false);

    IncrementStorageRevisionAndMakeLatest(gpuWritableStorage);

    if (GetUsage() & kMappableBufferUsages) {
        commandContext->AddBufferForSyncingWithCPU(this);
    }

    return {};
}

}  // namespace dawn::native::d3d11
