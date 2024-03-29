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

#include "dawn/native/SharedBufferMemory.h"

#include <utility>

#include "dawn/native/Buffer.h"
#include "dawn/native/ChainUtils.h"
#include "dawn/native/Device.h"
#include "dawn/native/Queue.h"

namespace dawn::native {

namespace {

class ErrorSharedBufferMemory : public SharedBufferMemoryBase {
  public:
    ErrorSharedBufferMemory(DeviceBase* device, const SharedBufferMemoryDescriptor* descriptor)
        : SharedBufferMemoryBase(device, descriptor, ObjectBase::kError) {}

    ResultOrError<Ref<BufferBase>> CreateBufferImpl(
        const UnpackedPtr<BufferDescriptor>& descriptor) override {
        DAWN_UNREACHABLE();
    }
    MaybeError BeginAccessImpl(BufferBase* buffer,
                               const UnpackedPtr<BeginAccessDescriptor>& descriptor) override {
        DAWN_UNREACHABLE();
    }
    ResultOrError<FenceAndSignalValue> EndAccessImpl(BufferBase* buffer,
                                                     UnpackedPtr<EndAccessState>& state) override {
        DAWN_UNREACHABLE();
    }
};

}  // namespace

// static
SharedBufferMemoryBase* SharedBufferMemoryBase::MakeError(
    DeviceBase* device,
    const SharedBufferMemoryDescriptor* descriptor) {
    return new ErrorSharedBufferMemory(device, descriptor);
}

SharedBufferMemoryBase::SharedBufferMemoryBase(DeviceBase* device,
                                               const SharedBufferMemoryDescriptor* descriptor,
                                               ObjectBase::ErrorTag tag)
    : ApiObjectBase(device, tag, descriptor->label),
      mProperties{nullptr, wgpu::BufferUsage::None, 0} {}

SharedBufferMemoryBase::SharedBufferMemoryBase(DeviceBase* device,
                                               const char* label,
                                               const SharedBufferMemoryProperties& properties)
    : ApiObjectBase(device, label), mProperties(properties) {
    GetObjectTrackingList()->Track(this);
}

ObjectType SharedBufferMemoryBase::GetType() const {
    return ObjectType::SharedBufferMemory;
}

void SharedBufferMemoryBase::DestroyImpl() {}

void SharedBufferMemoryBase::Initialize() {
    DAWN_ASSERT(!IsError());
    mContents = CreateContents();
}

void SharedBufferMemoryBase::APIGetProperties(SharedBufferMemoryProperties* properties) const {
    properties->usage = mProperties.usage;
    properties->size = mProperties.size;

    UnpackedPtr<SharedBufferMemoryProperties> unpacked;
    if (GetDevice()->ConsumedError(ValidateAndUnpack(properties), &unpacked,
                                   "calling %s.GetProperties", this)) {
        return;
    }
}

BufferBase* SharedBufferMemoryBase::APICreateBuffer(const BufferDescriptor* descriptor) {
    Ref<BufferBase> result;

    // Provide the defaults if no descriptor is provided.
    BufferDescriptor defaultDescriptor;
    if (descriptor == nullptr) {
        defaultDescriptor = {};
        defaultDescriptor.size = mProperties.size;
        defaultDescriptor.usage = mProperties.usage;
        descriptor = &defaultDescriptor;
    }

    if (GetDevice()->ConsumedError(CreateBuffer(descriptor), &result,
                                   InternalErrorType::OutOfMemory, "calling %s.CreateBuffer(%s).",
                                   this, descriptor)) {
        result = BufferBase::MakeError(GetDevice(), descriptor);
    }
    return ReturnToAPI(std::move(result));
}

ResultOrError<Ref<BufferBase>> SharedBufferMemoryBase::CreateBuffer(
    const BufferDescriptor* rawDescriptor) {
    DAWN_TRY(GetDevice()->ValidateIsAlive());
    DAWN_TRY(GetDevice()->ValidateObject(this));
    // Validate the buffer descriptor.
    UnpackedPtr<BufferDescriptor> descriptor;
    DAWN_TRY_ASSIGN(descriptor, ValidateBufferDescriptor(GetDevice(), rawDescriptor));

    // Ensure the buffer descriptor usage is a subset of the shared buffer memory's usage.
    DAWN_INVALID_IF(!IsSubset(descriptor->usage, mProperties.usage),
                    "The buffer usage (%s) is incompatible with the SharedBufferMemory usage (%s).",
                    descriptor->usage, mProperties.usage);

    // Validate that the buffer size exactly matches the shared buffer memory's size.
    DAWN_INVALID_IF(descriptor->size != mProperties.size,
                    "SharedBufferMemory size (%u) doesn't match descriptor size (%u).",
                    mProperties.size, descriptor->size);

    Ref<BufferBase> buffer;
    DAWN_TRY_ASSIGN(buffer, CreateBufferImpl(descriptor));
    // Access is not allowed until BeginAccess has been called.
    buffer->SetHasAccess(false);
    return buffer;
}

Ref<SharedBufferMemoryContents> SharedBufferMemoryBase::CreateContents() {
    return AcquireRef(new SharedBufferMemoryContents(GetWeakRef(this)));
}

SharedBufferMemoryContents* SharedBufferMemoryBase::GetContents() const {
    return mContents.Get();
}

MaybeError SharedBufferMemoryBase::ValidateBufferCreatedFromSelf(BufferBase* buffer) {
    auto* contents = buffer->GetSharedBufferMemoryContents();
    DAWN_INVALID_IF(contents == nullptr, "%s was not created from %s.", buffer, this);

    auto* sharedBufferMemory =
        buffer->GetSharedBufferMemoryContents()->GetSharedBufferMemory().Promote().Get();
    DAWN_INVALID_IF(sharedBufferMemory != this, "%s created from %s cannot be used with %s.",
                    buffer, sharedBufferMemory, this);
    return {};
}

bool SharedBufferMemoryBase::APIBeginAccess(BufferBase* buffer,
                                            const BeginAccessDescriptor* descriptor) {
    if (GetDevice()->ConsumedError(BeginAccess(buffer, descriptor), "calling %s.BeginAccess(%s).",
                                   this, buffer)) {
        return false;
    }
    return true;
}

MaybeError SharedBufferMemoryBase::BeginAccess(BufferBase* buffer,
                                               const BeginAccessDescriptor* rawDescriptor) {
    UnpackedPtr<BeginAccessDescriptor> descriptor;
    DAWN_TRY_ASSIGN(descriptor, ValidateAndUnpack(rawDescriptor));

    DAWN_TRY(GetDevice()->ValidateIsAlive());
    DAWN_TRY(GetDevice()->ValidateObject(buffer));
    for (size_t i = 0; i < descriptor->fenceCount; ++i) {
        DAWN_TRY(GetDevice()->ValidateObject(descriptor->fences[i]));
    }

    // Validate there is not another ongoing access and then set the current access.
    // This is done first because BeginAccess should acquire access regardless of whether or
    // not the internals generate an error.
    DAWN_INVALID_IF(mCurrentAccess != nullptr,
                    "Cannot begin access with %s on %s which is currently accessed by %s.", buffer,
                    this, mCurrentAccess.Get());

    DAWN_INVALID_IF(buffer->HasAccess(), "%s is already used to access %s.", buffer, this);

    DAWN_TRY(ValidateBufferCreatedFromSelf(buffer));

    DAWN_TRY(BeginAccessImpl(buffer, descriptor));

    // Append begin fences first. Fences should be tracked regardless of whether later errors occur.
    for (size_t i = 0; i < descriptor->fenceCount; ++i) {
        mContents->mPendingFences->push_back(
            {descriptor->fences[i], descriptor->signaledValues[i]});
    }

    DAWN_ASSERT(!buffer->IsError());
    buffer->SetHasAccess(true);
    if (descriptor->initialized) {
        buffer->SetIsDataInitialized();
    }
    mCurrentAccess = buffer;

    return {};
}

bool SharedBufferMemoryBase::APIEndAccess(BufferBase* buffer, EndAccessState* state) {
    if (GetDevice()->ConsumedError(EndAccess(buffer, state), "calling %s.EndAccess(%s).", this,
                                   buffer)) {
        return false;
    }
    return true;
}

MaybeError SharedBufferMemoryBase::EndAccess(BufferBase* buffer, EndAccessState* state) {
    DAWN_TRY(GetDevice()->ValidateObject(buffer));
    DAWN_TRY(ValidateBufferCreatedFromSelf(buffer));

    DAWN_INVALID_IF(mCurrentAccess != buffer,
                    "Cannot end access with %s on %s which is currently accessed by %s.", buffer,
                    this, mCurrentAccess.Get());

    DAWN_INVALID_IF(!buffer->HasAccess(), "%s is not currently being accessed.", buffer);

    DAWN_INVALID_IF(buffer->APIGetMapState() != wgpu::BufferMapState::Unmapped,
                    "%s is currently mapped or pending map.", buffer);

    PendingFenceList fenceList;
    mContents->AcquirePendingFences(&fenceList);

    // Call the error-generating part of the EndAccess implementation. This is separated out because
    // writing the output state must happen regardless of whether or not EndAccessInternal
    // succeeds.
    MaybeError err;
    {
        ResultOrError<FenceAndSignalValue> result = EndAccessInternal(buffer, state);
        if (result.IsSuccess()) {
            fenceList->push_back(result.AcquireSuccess());
        } else {
            err = result.AcquireError();
        }
    }

    // Copy the fences to the output state.
    if (size_t fenceCount = fenceList->size()) {
        auto* fences = new SharedFenceBase*[fenceCount];
        uint64_t* signaledValues = new uint64_t[fenceCount];
        for (size_t i = 0; i < fenceCount; ++i) {
            fences[i] = fenceList[i].object.Detach();
            signaledValues[i] = fenceList[i].signaledValue;
        }

        state->fenceCount = fenceCount;
        state->fences = fences;
        state->signaledValues = signaledValues;
    } else {
        state->fenceCount = 0;
        state->fences = nullptr;
        state->signaledValues = nullptr;
    }

    state->initialized = buffer->IsDataInitialized();
    buffer->SetHasAccess(false);
    mCurrentAccess = nullptr;

    return err;
}

ResultOrError<FenceAndSignalValue> SharedBufferMemoryBase::EndAccessInternal(
    BufferBase* buffer,
    EndAccessState* rawState) {
    UnpackedPtr<EndAccessState> state;
    DAWN_TRY_ASSIGN(state, ValidateAndUnpack(rawState));
    // Ensure that commands are submitted before exporting fences with the last usage serial.
    DAWN_TRY(GetDevice()->GetQueue()->EnsureCommandsFlushed(mCurrentAccess->GetLastUsageSerial()));
    return EndAccessImpl(buffer, state);
}

bool SharedBufferMemoryBase::APIIsDeviceLost() {
    return GetDevice()->IsLost();
}

SharedBufferMemoryContents::SharedBufferMemoryContents(
    WeakRef<SharedBufferMemoryBase> sharedBufferMemory)
    : mSharedBufferMemory(std::move(sharedBufferMemory)) {}

const WeakRef<SharedBufferMemoryBase>& SharedBufferMemoryContents::GetSharedBufferMemory() const {
    return mSharedBufferMemory;
}

void SharedBufferMemoryContents::AcquirePendingFences(PendingFenceList* fences) {
    *fences = mPendingFences;
    mPendingFences->clear();
}

void APISharedBufferMemoryEndAccessStateFreeMembers(WGPUSharedBufferMemoryEndAccessState cState) {
    auto* state = reinterpret_cast<SharedBufferMemoryBase::EndAccessState*>(&cState);
    for (size_t i = 0; i < state->fenceCount; ++i) {
        state->fences[i]->APIRelease();
    }
    delete[] state->fences;
    delete[] state->signaledValues;
}

}  // namespace dawn::native
