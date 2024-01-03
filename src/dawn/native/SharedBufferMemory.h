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

#ifndef SRC_DAWN_NATIVE_SHAREDBUFFERMEMORY_H_
#define SRC_DAWN_NATIVE_SHAREDBUFFERMEMORY_H_

#include <map>
#include <stack>

#include "dawn/common/StackContainer.h"
#include "dawn/common/WeakRef.h"
#include "dawn/common/WeakRefSupport.h"
#include "dawn/native/Error.h"
#include "dawn/native/Forward.h"
#include "dawn/native/IntegerTypes.h"
#include "dawn/native/ObjectBase.h"
#include "dawn/native/SharedFence.h"
#include "dawn/native/dawn_platform.h"

namespace dawn::native {

class SharedBufferMemoryContents;
struct SharedBufferMemoryDescriptor;
struct SharedBufferMemoryBeginAccessDescriptor;
struct SharedBufferMemoryEndAccessState;
struct SharedBufferMemoryProperties;
struct BufferDescriptor;

class SharedBufferMemoryBase : public ApiObjectBase, public WeakRefSupport<SharedBufferMemoryBase> {
  public:
    using BeginAccessDescriptor = SharedBufferMemoryBeginAccessDescriptor;
    using EndAccessState = SharedBufferMemoryEndAccessState;
    using PendingFenceList = StackVector<FenceAndSignalValue, 1>;

    static SharedBufferMemoryBase* MakeError(DeviceBase* device,
                                             const SharedBufferMemoryDescriptor* descriptor);

    // Strip out properties based on what the Dawn device actually supports.
    static void ReifyProperties(DeviceBase* device, SharedBufferMemoryProperties* properties);

    void Initialize();

    void APIGetProperties(SharedBufferMemoryProperties* properties) const;
    BufferBase* APICreateBuffer(const BufferDescriptor* descriptor);
    // Returns true if access was acquired. If it returns true, then APIEndAccess must
    // be called to release access. Other errors may occur even if `true` is returned.
    // Use an error scope to catch them.
    bool APIBeginAccess(BufferBase* buffer, const BeginAccessDescriptor* descriptor);
    // Returns true if access was released.
    bool APIEndAccess(BufferBase* buffer, EndAccessState* state);
    // Returns true iff the device passed to this object on creation is now lost.
    // TODO(crbug.com/1506468): Eliminate this API once Chromium has been
    // transitioned away from using it in favor of observing device lost events.
    bool APIIsDeviceLost();

    ObjectType GetType() const override;

    SharedBufferMemoryContents* GetContents() const;

    // Validate that the buffer was created from this SharedBufferMemory.
    MaybeError ValidateBufferCreatedFromSelf(BufferBase* buffer);

  protected:
    SharedBufferMemoryBase(DeviceBase* device,
                           const char* label,
                           const SharedBufferMemoryProperties& properties);
    SharedBufferMemoryBase(DeviceBase* device,
                           const SharedBufferMemoryDescriptor* descriptor,
                           ObjectBase::ErrorTag tag);

    void DestroyImpl() override;

    SharedBufferMemoryProperties mProperties;

    Ref<BufferBase> mCurrentAccess;

  private:
    virtual Ref<SharedBufferMemoryContents> CreateContents();

    ResultOrError<Ref<BufferBase>> CreateBuffer(const BufferDescriptor* rawDescriptor);
    MaybeError BeginAccess(BufferBase* buffer, const BeginAccessDescriptor* rawDescriptor);
    MaybeError EndAccess(BufferBase* buffer, EndAccessState* state);
    ResultOrError<FenceAndSignalValue> EndAccessInternal(BufferBase* buffer,
                                                         EndAccessState* rawState);

    virtual ResultOrError<Ref<BufferBase>> CreateBufferImpl(
        const UnpackedPtr<BufferDescriptor>& descriptor) = 0;

    // BeginAccessImpl validates the operation is valid on the backend, and performs any
    // backend specific operations. It does NOT need to acquire begin fences; that is done in the
    // frontend in BeginAccess.
    virtual MaybeError BeginAccessImpl(BufferBase* buffer,
                                       const UnpackedPtr<BeginAccessDescriptor>& descriptor) = 0;
    // EndAccessImpl validates the operation is valid on the backend, and returns the end fence.
    // It should also write out any backend specific state in chained out structs of EndAccessState.
    virtual ResultOrError<FenceAndSignalValue> EndAccessImpl(
        BufferBase* buffer,
        UnpackedPtr<EndAccessState>& state) = 0;

    Ref<SharedBufferMemoryContents> mContents;
};

// SharedTextureMemoryContents is a separate object because it needs to live as long as
// the SharedTextureMemory or any textures created from the SharedTextureMemory. This
// allows state and objects needed by the texture to persist after the
// SharedTextureMemory itself has been dropped.
class SharedBufferMemoryContents : public RefCounted {
  public:
    using PendingFenceList = SharedBufferMemoryBase::PendingFenceList;

    explicit SharedBufferMemoryContents(WeakRef<SharedBufferMemoryBase> sharedBufferMemory);

    void AcquirePendingFences(PendingFenceList* fences);

    // Set the last usage serial. This indicates when the SharedFence exported
    // from APIEndAccess will complete.
    void SetLastUsageSerial(ExecutionSerial lastUsageSerial);
    ExecutionSerial GetLastUsageSerial() const;

    const WeakRef<SharedBufferMemoryBase>& GetSharedBufferMemory() const;

  private:
    friend class SharedBufferMemoryBase;

    PendingFenceList mPendingFences;
    ExecutionSerial mLastUsageSerial{0};

    WeakRef<SharedBufferMemoryBase> mSharedBufferMemory;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_SHAREDBUFFERMEMORY_H_
