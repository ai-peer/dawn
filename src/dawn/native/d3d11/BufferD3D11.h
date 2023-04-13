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

#ifndef SRC_DAWN_NATIVE_D3D11_BUFFERD3D11_H_
#define SRC_DAWN_NATIVE_D3D11_BUFFERD3D11_H_

#include <limits>
#include <memory>

#include "dawn/native/Buffer.h"
#include "dawn/native/d3d/d3d_platform.h"

namespace dawn::native::d3d11 {

class CommandRecordingContext;
class Device;

class Buffer final : public BufferBase {
  public:
    static ResultOrError<Ref<Buffer>> Create(Device* device, const BufferDescriptor* descriptor);

    MaybeError EnsureDataInitialized(CommandRecordingContext* commandContext);
    ResultOrError<bool> EnsureDataInitializedAsDestination(CommandRecordingContext* commandContext,
                                                           uint64_t offset,
                                                           uint64_t size);
    MaybeError EnsureDataInitializedAsDestination(CommandRecordingContext* commandContext,
                                                  const CopyTextureToBufferCmd* copy);

    // Dawn API
    void SetLabelImpl() override;

    ID3D11Buffer* GetD3D11Buffer() const { return mD3d11Buffer.Get(); }
    ResultOrError<ComPtr<ID3D11ShaderResourceView>> CreateD3D11ShaderResourceView(
        uint64_t offset,
        uint64_t size) const;
    ResultOrError<ComPtr<ID3D11UnorderedAccessView1>> CreateD3D11UnorderedAccessView1(
        uint64_t offset,
        uint64_t size) const;

    MaybeError Clear(CommandRecordingContext* commandContext,
                     uint8_t clearValue,
                     uint64_t offset,
                     uint64_t size);
    MaybeError Write(CommandRecordingContext* commandContext,
                     uint64_t offset,
                     const void* data,
                     size_t size);
    MaybeError CopyFromBuffer(CommandRecordingContext* commandContext,
                              uint64_t offset,
                              size_t size,
                              Buffer* source,
                              uint64_t sourceOffset);

    uint8_t* GetMappedData() const;

  private:
    Buffer(Device* device, const BufferDescriptor* descriptor);
    ~Buffer() override;

    MaybeError Initialize(bool mappedAtCreation);
    MaybeError MapAsyncImpl(wgpu::MapMode mode, size_t offset, size_t size) override;
    void UnmapImpl() override;
    void DestroyImpl() override;
    bool IsCPUWritableAtCreation() const override;
    MaybeError MapAtCreationImpl() override;
    void* GetMappedPointer() override;

    MaybeError MapInternal(const char* contextInfo);
    void UnmapInternal();

    MaybeError InitializeToZero(CommandRecordingContext* commandContext);
    // CLear the buffer without checking if the buffer is initialized.
    MaybeError ClearInternal(CommandRecordingContext* commandContext,
                             uint8_t clearValue,
                             uint64_t offset = 0,
                             uint64_t size = 0);
    // Write the buffer without checking if the buffer is initialized.
    MaybeError WriteInternal(CommandRecordingContext* commandContext,
                             uint64_t bufferOffset,
                             const void* data,
                             size_t size);

    // The buffer object can be used as vertex, index, uniform, storage, or indirect buffer.
    ComPtr<ID3D11Buffer> mD3d11Buffer;
    uint8_t* mMappedData = nullptr;
};

}  // namespace dawn::native::d3d11

#endif  // SRC_DAWN_NATIVE_D3D11_BUFFERD3D11_H_
