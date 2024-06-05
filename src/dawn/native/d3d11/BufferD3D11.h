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

#ifndef SRC_DAWN_NATIVE_D3D11_BUFFERD3D11_H_
#define SRC_DAWN_NATIVE_D3D11_BUFFERD3D11_H_

#include <limits>
#include <memory>

#include "dawn/native/Buffer.h"
#include "dawn/native/d3d/d3d_platform.h"
#include "partition_alloc/pointers/raw_ptr.h"

namespace dawn::native::d3d11 {

class Device;
class ScopedCommandRecordingContext;

class Buffer : public BufferBase {
  public:
    static ResultOrError<Ref<Buffer>> Create(Device* device,
                                             const UnpackedPtr<BufferDescriptor>& descriptor,
                                             const ScopedCommandRecordingContext* commandContext,
                                             bool allowUploadBufferEmulation = true);

    MaybeError EnsureDataInitialized(const ScopedCommandRecordingContext* commandContext);
    MaybeError EnsureDataInitializedAsDestination(
        const ScopedCommandRecordingContext* commandContext,
        uint64_t offset,
        uint64_t size);
    MaybeError EnsureDataInitializedAsDestination(
        const ScopedCommandRecordingContext* commandContext,
        const CopyTextureToBufferCmd* copy);

    // Dawn API
    void SetLabelImpl() override;

    MaybeError SyncCPUAccessibleStorages(const ScopedCommandRecordingContext* commandContext);

    // The buffer object for constant buffer usage.
    ResultOrError<ID3D11Buffer*> GetAndSyncD3D11ConstantBuffer(
        const ScopedCommandRecordingContext* commandContext);
    // Get constant buffer without any sync. Note this function is only allowed if this object only
    // contains constant buffer storage.
    ID3D11Buffer* GetD3D11ConstantBuffer();
    // The buffer object for GPU read excluding constant buffer usages.
    ResultOrError<ID3D11Buffer*> GetAndSyncD3D11GPUReadBuffer(
        const ScopedCommandRecordingContext* commandContext);

    ResultOrError<ComPtr<ID3D11ShaderResourceView>> CreateD3D11ShaderResourceView(
        const ScopedCommandRecordingContext* commandContext,
        uint64_t offset,
        uint64_t size);
    ResultOrError<ComPtr<ID3D11UnorderedAccessView1>> CreateD3D11UnorderedAccessView1(
        const ScopedCommandRecordingContext* commandContext,
        uint64_t offset,
        uint64_t size);
    MaybeError Clear(const ScopedCommandRecordingContext* commandContext,
                     uint8_t clearValue,
                     uint64_t offset,
                     uint64_t size);
    MaybeError Write(const ScopedCommandRecordingContext* commandContext,
                     uint64_t offset,
                     const void* data,
                     size_t size);

    static MaybeError Copy(const ScopedCommandRecordingContext* commandContext,
                           Buffer* source,
                           uint64_t sourceOffset,
                           size_t size,
                           Buffer* destination,
                           uint64_t destinationOffset);

    // Actually map the buffer when its last usage serial has passed.
    MaybeError FinalizeMap(ScopedCommandRecordingContext* commandContext,
                           ExecutionSerial completedSerial,
                           wgpu::MapMode mode);

    bool IsCPUWritable() const;
    bool IsCPUReadable() const;

    class ScopedMap : public NonCopyable {
      public:
        // Map buffer and return a ScopedMap object. If the buffer is not mappable,
        // scopedMap.GetMappedData() will return nullptr.
        static ResultOrError<ScopedMap> Create(const ScopedCommandRecordingContext* commandContext,
                                               Buffer* buffer,
                                               wgpu::MapMode mode);

        ScopedMap();
        ~ScopedMap();

        ScopedMap(ScopedMap&& other);
        ScopedMap& operator=(ScopedMap&& other);

        uint8_t* GetMappedData() const;

        void Reset();

      private:
        ScopedMap(const ScopedCommandRecordingContext* commandContext,
                  Buffer* buffer,
                  bool needsUnmap);

        raw_ptr<const ScopedCommandRecordingContext> mCommandContext = nullptr;
        raw_ptr<Buffer> mBuffer = nullptr;
        // Whether the buffer needs to be unmapped when the ScopedMap object is destroyed.
        bool mNeedsUnmap = false;
    };

  protected:
    using BufferBase::BufferBase;

    ~Buffer() override;

    virtual MaybeError InitializeInternal();

    virtual MaybeError MapInternal(const ScopedCommandRecordingContext* commandContext,
                                   wgpu::MapMode mode);
    virtual void UnmapInternal(const ScopedCommandRecordingContext* commandContext);

    // Clear the buffer without checking if the buffer is initialized.
    virtual MaybeError ClearInternal(const ScopedCommandRecordingContext* commandContext,
                                     uint8_t clearValue,
                                     uint64_t offset = 0,
                                     uint64_t size = 0);

    virtual uint8_t* GetUploadData() const;

    raw_ptr<uint8_t, AllowPtrArithmetic> mMappedData = nullptr;

  private:
    MaybeError Initialize(bool mappedAtCreation,
                          const ScopedCommandRecordingContext* commandContext);
    MaybeError MapAsyncImpl(wgpu::MapMode mode, size_t offset, size_t size) override;
    void UnmapImpl() override;
    void DestroyImpl() override;
    bool IsCPUWritableAtCreation() const override;
    MaybeError MapAtCreationImpl() override;
    void* GetMappedPointer() override;

    MaybeError InitializeToZero(const ScopedCommandRecordingContext* commandContext);
    // Write the buffer without checking if the buffer is initialized.
    MaybeError WriteInternal(const ScopedCommandRecordingContext* commandContext,
                             uint64_t bufferOffset,
                             const void* data,
                             size_t size);
    // Copy the buffer without checking if the buffer is initialized.
    static MaybeError CopyInternal(const ScopedCommandRecordingContext* commandContext,
                                   Buffer* source,
                                   uint64_t sourceOffset,
                                   size_t size,
                                   Buffer* destination,
                                   uint64_t destinationOffset);

    MaybeError AllocateStagingStorageIfNeeded();
    MaybeError AllocateGPUWriteStorageIfNeeded();

    class Storage;
    // Update dstStorage to latest revision
    MaybeError SyncStorage(const ScopedCommandRecordingContext* commandContext,
                           Storage* dstStorage);
    void IncrementStorageRevisionAndMakeLatest(Storage* dstStorage);

    Storage* GetCopySrcStorage();
    ResultOrError<Storage*> GetCopyDstStorage();

    // Various buffer objects for different purposes. Note some of them might reference
    // the same underlying buffer object.
    // The buffer object for constant buffer usage.
    Ref<Storage> mConstantBufferStorage;
    // The buffer object for CPU Write.
    Ref<Storage> mCPUWriteStorage;
    // The buffer object for staging.
    Ref<Storage> mStagingStorage;
    // The buffer object for GPU write.
    Ref<Storage> mGPUWriteStorage;
    // The buffer object for GPU read excluding constant buffer usages.
    Ref<Storage> mGPUReadStorage;

    Storage* mLastUpdatedStorage = nullptr;
    Storage* mMappedStorage = nullptr;

    ExecutionSerial mMapReadySerial = kMaxExecutionSerial;
};

}  // namespace dawn::native::d3d11

#endif  // SRC_DAWN_NATIVE_D3D11_BUFFERD3D11_H_
