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

#ifndef SRC_DAWN_NATIVE_D3D11_MAPPABLEBUFFERD3D11_H_
#define SRC_DAWN_NATIVE_D3D11_MAPPABLEBUFFERD3D11_H_

#include <memory>
#include <utility>

#include "dawn/common/Ref.h"
#include "dawn/common/ityp_array.h"
#include "dawn/native/d3d11/BufferD3D11.h"
#include "partition_alloc/pointers/raw_ptr.h"

namespace dawn::native::d3d11 {

// A subclass of Buffer that supports mapping on non-staging buffers. It's achieveded by managing
// several copies of the buffer, each with its own ID3D11Buffer storage for specific usage.
// For example, a buffer that has MapWrite + Storage usage will have at least two copies:
//  - One copy with D3D11_USAGE_DYNAMIC for mapping on CPU.
//  - One copy with D3D11_USAGE_DEFAULT for writing on GPU.
// Internally this class will synchronize the content between the copies so that when it is mapped
// or used by GPU, the appropriate copy will have the up-to-date content. The synchronizations are
// done in a way that minimizes CPU stall as much as possible.
class MappableBuffer final : public GPUUsableBuffer {
  public:
    MappableBuffer(DeviceBase* device, const UnpackedPtr<BufferDescriptor>& descriptor);
    ~MappableBuffer() override;

    ResultOrError<ID3D11Buffer*> GetD3D11ConstantBuffer(
        const ScopedCommandRecordingContext* commandContext) override;
    ResultOrError<ID3D11Buffer*> GetD3D11NonConstantBuffer(
        const ScopedCommandRecordingContext* commandContext) override;

    ResultOrError<ComPtr<ID3D11ShaderResourceView>> UseAsSRV(
        const ScopedCommandRecordingContext* commandContext,
        uint64_t offset,
        uint64_t size) override;
    ResultOrError<ComPtr<ID3D11UnorderedAccessView1>> UseAsUAV(
        const ScopedCommandRecordingContext* commandContext,
        uint64_t offset,
        uint64_t size) override;

    MaybeError PredicatedClear(const ScopedSwapStateCommandRecordingContext* commandContext,
                               ID3D11Predicate* predicate,
                               uint8_t clearValue,
                               uint64_t offset,
                               uint64_t size) override;

    // Make sure CPU accessible storages are up-to-date. This is usually called at the end of a
    // command buffer after the buffer was modified on GPU.
    MaybeError SyncCPUAccessibleStorages(const ScopedCommandRecordingContext* commandContext);

  private:
    class Storage;

    // Dawn API
    void DestroyImpl() override;
    void SetLabelImpl() override;

    MaybeError InitializeInternal() override;
    MaybeError MapInternal(const ScopedCommandRecordingContext* commandContext,
                           wgpu::MapMode mode) override;
    void UnmapInternal(const ScopedCommandRecordingContext* commandContext) override;

    MaybeError CopyToInternal(const ScopedCommandRecordingContext* commandContext,
                              uint64_t sourceOffset,
                              size_t size,
                              Buffer* destination,
                              uint64_t destinationOffset) override;
    MaybeError CopyFromD3DInternal(const ScopedCommandRecordingContext* commandContext,
                                   ID3D11Buffer* srcD3D11Buffer,
                                   uint64_t sourceOffset,
                                   size_t size,
                                   uint64_t destinationOffset) override;

    MaybeError WriteInternal(const ScopedCommandRecordingContext* commandContext,
                             uint64_t bufferOffset,
                             const void* data,
                             size_t size) override;

    // Storage types for copies.
    enum class StorageType : uint8_t {
        // Storage for write mapping with constant buffer usage,
        CPUWritableConstantBuffer,
        // Storage for CopyB2B with destination having constant buffer usage,
        GPUCopyDstConstantBuffer,
        // Storage for write mapping with other usages (non-constant buffer),
        CPUWritableNonConstantBuffer,
        // Storage for GPU writing with other usages (non-constant buffer),
        GPUWritableNonConstantBuffer,
        // Storage for staging usage,
        Staging,

        Count,
    };

    ResultOrError<Storage*> AllocateStorageIfNeeded(StorageType storageType);
    // Get or create storage supporting CopyDst usage.
    ResultOrError<Storage*> AllocateDstCopyableStorageIfNeeded();

    void SetStorageLabel(StorageType storageType);

    // Update dstStorage to latest revision
    MaybeError SyncStorage(const ScopedCommandRecordingContext* commandContext,
                           Storage* dstStorage);
    void IncrementStorageRevisionAndMakeLatest(Storage* dstStorage);

    using StorageMap =
        ityp::array<StorageType, Ref<Storage>, static_cast<uint8_t>(StorageType::Count)>;

    StorageMap mStorages;

    // The storage contains most up-to-date content.
    raw_ptr<Storage> mLastUpdatedStorage;
    // This points to either CPU writable constant buffer or CPU writable non-constant buffer. We
    // don't need both to exist.
    raw_ptr<Storage> mCPUWritableStorage;
    raw_ptr<Storage> mMappedStorage;
};

}  // namespace dawn::native::d3d11

#endif
