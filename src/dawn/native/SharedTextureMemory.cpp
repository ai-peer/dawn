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

#include "dawn/native/SharedTextureMemory.h"

#include <utility>

#include "dawn/native/ChainUtils_autogen.h"
#include "dawn/native/Device.h"
#include "dawn/native/SharedFence.h"
#include "dawn/native/dawn_platform.h"

namespace dawn::native {

// static
SharedTextureMemoryBase* SharedTextureMemoryBase::MakeError(
    DeviceBase* device,
    const SharedTextureMemoryDescriptor* descriptor) {
    return new SharedTextureMemoryBase(device, descriptor, ObjectBase::kError);
}

SharedTextureMemoryBase::SharedTextureMemoryBase(DeviceBase* device,
                                                 const SharedTextureMemoryDescriptor* descriptor,
                                                 ObjectBase::ErrorTag tag)
    : ApiObjectBase(device, tag, descriptor->label), mProperties() {}

SharedTextureMemoryBase::SharedTextureMemoryBase(DeviceBase* device,
                                                 const char* label,
                                                 const SharedTextureMemoryProperties& properties)
    : ApiObjectBase(device, label), mProperties(properties) {
    const Format& internalFormat = device->GetValidInternalFormat(properties.format);
    if (!internalFormat.supportsStorageUsage) {
        ASSERT(!(mProperties.usage & wgpu::TextureUsage::StorageBinding));
    }
    if (!internalFormat.isRenderable) {
        ASSERT(!(mProperties.usage & wgpu::TextureUsage::RenderAttachment));
    }
    GetObjectTrackingList()->Track(this);
}

ObjectType SharedTextureMemoryBase::GetType() const {
    return ObjectType::SharedTextureMemory;
}

void SharedTextureMemoryBase::DestroyImpl() {}

void SharedTextureMemoryBase::APIGetProperties(SharedTextureMemoryProperties* properties) const {
    properties->usage = mProperties.usage;
    properties->size = mProperties.size;
    properties->format = mProperties.format;

    if (GetDevice()->ConsumedError(
            [&]() -> MaybeError {
                DAWN_TRY(GetDevice()->ValidateObject(this));
                DAWN_TRY(ValidateSTypes(properties->nextInChain, {}));
                return {};
            }(),
            "calling %s.GetProperties", this)) {
        return;
    }
}

TextureBase* SharedTextureMemoryBase::APICreateTexture(const TextureDescriptor* descriptor) {
    Ref<TextureBase> result;
    if (GetDevice()->ConsumedError(CreateTexture(descriptor), &result,
                                   InternalErrorType::OutOfMemory, "calling %s.CreateTexture(%s).",
                                   this, descriptor)) {
        return TextureBase::MakeError(GetDevice(), descriptor);
    }
    return result.Detach();
}

ResultOrError<Ref<TextureBase>> SharedTextureMemoryBase::CreateTexture(
    const TextureDescriptor* descriptor) {
    DAWN_TRY(GetDevice()->ValidateIsAlive());
    DAWN_TRY(GetDevice()->ValidateObject(this));

    DAWN_INVALID_IF(descriptor->dimension != wgpu::TextureDimension::e2D,
                    "Texture dimension (%s) is not %s.", descriptor->dimension,
                    wgpu::TextureDimension::e2D);

    DAWN_INVALID_IF(descriptor->mipLevelCount != 1, "Mip level count (%u) is not 1.",
                    descriptor->mipLevelCount);

    DAWN_INVALID_IF(descriptor->size.depthOrArrayLayers != 1, "Array layer count (%u) is not 1.",
                    descriptor->size.depthOrArrayLayers);

    DAWN_INVALID_IF(descriptor->sampleCount != 1, "Sample count (%u) is not 1.",
                    descriptor->sampleCount);

    DAWN_INVALID_IF(
        (descriptor->size.width != mProperties.size.width) ||
            (descriptor->size.height != mProperties.size.height) ||
            (descriptor->size.depthOrArrayLayers != mProperties.size.depthOrArrayLayers),
        "SharedTextureMemory size (%s) doesn't match descriptor size (%s).", &mProperties.size,
        &descriptor->size);

    DAWN_INVALID_IF(descriptor->format != mProperties.format,
                    "SharedTextureMemory format (%s) doesn't match descriptor format (%s).",
                    mProperties.format, descriptor->format);

    DAWN_TRY(ValidateTextureDescriptor(GetDevice(), descriptor, AllowMultiPlanarTextureFormat::Yes,
                                       mProperties.usage));

    return CreateTextureImpl(descriptor);
}

ResultOrError<Ref<TextureBase>> SharedTextureMemoryBase::CreateTextureImpl(
    const TextureDescriptor* descriptor) {
    return DAWN_UNIMPLEMENTED_ERROR("Not implemented");
}

bool SharedTextureMemoryBase::CheckCurrentAccess(const TextureBase* texture) const {
    return texture == mCurrentAccess.Get();
}

void SharedTextureMemoryBase::BeginAccessScope(TextureBase* texture,
                                               const BeginAccessDescriptor* descriptor) {
    PendingFenceList fences;
    for (size_t i = 0; i < descriptor->fenceCount; ++i) {
        fences->push_back({descriptor->fences[i], descriptor->signaledValues[i]});
    }
    mAccessScopes->push_back(AccessScope{texture, fences});
}

// Helper for use in AcquireBeginFences / EndAccessScope. Finds the latest AccessScope for
// `texture`.
template <typename AccessScope, size_t N>
static auto FindAccessScope(TextureBase* texture, StackVector<AccessScope, N>& scopes) {
    // Likely case where there is only one access scope.
    if (DAWN_LIKELY(scopes->size() == 1 && scopes[0].texture == texture)) {
        return scopes->begin();
    }

    // Search from the back, the most recently pushed scope.
    for (auto it = scopes->rbegin(); it != scopes->rend(); ++it) {
        if (it->texture == texture) {
            return (++it).base();
        }
    }
    return scopes->end();
}

void SharedTextureMemoryBase::AcquireBeginFences(TextureBase* texture, PendingFenceList* fences) {
    auto it = FindAccessScope(texture, mAccessScopes);
    if (it != mAccessScopes->end()) {
        *fences = it->pendingBeginFences;
        it->pendingBeginFences->clear();
    }
}

void SharedTextureMemoryBase::EndAccessScope(TextureBase* texture, PendingFenceList* fences) {
    auto it = FindAccessScope(texture, mAccessScopes);
    if (it != mAccessScopes->end()) {
        *fences = it->pendingBeginFences;
        mAccessScopes->erase(it);
    }
}

void SharedTextureMemoryBase::APIBeginAccess(TextureBase* texture,
                                             const BeginAccessDescriptor* descriptor) {
    DAWN_UNUSED(GetDevice()->ConsumedError(BeginAccess(texture, descriptor),
                                           "calling %s.BeginAccess(%s).", this, texture));
}

MaybeError SharedTextureMemoryBase::BeginAccess(TextureBase* texture,
                                                const BeginAccessDescriptor* descriptor) {
    BeginAccessScope(texture, descriptor);

    DAWN_INVALID_IF(mCurrentAccess != nullptr,
                    "Cannot begin access with %s on %s which is currently accessed by %s.", texture,
                    this, mCurrentAccess.Get());
    mCurrentAccess = texture;

    DAWN_TRY(GetDevice()->ValidateIsAlive());
    DAWN_TRY(GetDevice()->ValidateObject(texture));
    for (size_t i = 0; i < descriptor->fenceCount; ++i) {
        DAWN_TRY(GetDevice()->ValidateObject(descriptor->fences[i]));
    }

    Ref<SharedTextureMemoryBase> memory = texture->QuerySharedTextureMemory();
    DAWN_INVALID_IF(memory.Get() != this, "%s was created from %s and cannot be used with %s.",
                    texture, memory.Get(), this);

    DAWN_INVALID_IF(texture->GetFormat().IsMultiPlanar() & !descriptor->initialized,
                    "BeginAccess on %s with multiplanar format (%s) must be initialized.", texture,
                    texture->GetFormat().format);

    DAWN_TRY(BeginAccessImpl(texture, descriptor));
    if (!texture->IsError()) {
        texture->SetIsSubresourceContentInitialized(descriptor->initialized,
                                                    texture->GetAllSubresources());
    }
    return {};
}

MaybeError SharedTextureMemoryBase::BeginAccessImpl(TextureBase* texture,
                                                    const BeginAccessDescriptor*) {
    return DAWN_UNIMPLEMENTED_ERROR("Not implemented");
}

void SharedTextureMemoryBase::APIEndAccess(TextureBase* texture, EndAccessState* state) {
    DAWN_UNUSED(GetDevice()->ConsumedError(EndAccess(texture, state), "calling %s.EndAccess(%s).",
                                           this, texture));
}

MaybeError SharedTextureMemoryBase::EndAccess(TextureBase* texture, EndAccessState* state) {
    PendingFenceList fenceList;
    EndAccessScope(texture, &fenceList);

    // Call the error-generating part of the EndAccess implementation. This is separated out because
    // writing the output state must happen regardless of whether or not EndAccessInternal
    // succeeds.
    MaybeError err;
    {
        ResultOrError<FenceAndSignalValue> result = EndAccessInternal(texture, state);
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
    state->initialized = texture->IsError() ||
                         texture->IsSubresourceContentInitialized(texture->GetAllSubresources());
    return err;
}

ResultOrError<FenceAndSignalValue> SharedTextureMemoryBase::EndAccessInternal(
    TextureBase* texture,
    EndAccessState* state) {
    DAWN_INVALID_IF(mCurrentAccess != texture,
                    "Cannot end access with %s on %s which is currently accessed by %s.", texture,
                    this, mCurrentAccess.Get());
    mCurrentAccess = nullptr;

    DAWN_TRY(GetDevice()->ValidateObject(texture));

    Ref<SharedTextureMemoryBase> memory = texture->QuerySharedTextureMemory();
    DAWN_INVALID_IF(memory.Get() != this, "%s was created from %s and cannot be used with %s.",
                    texture, memory.Get(), this);

    return EndAccessImpl(texture);
}

ResultOrError<FenceAndSignalValue> SharedTextureMemoryBase::EndAccessImpl(TextureBase* texture) {
    return DAWN_UNIMPLEMENTED_ERROR("Not implemented");
}

}  // namespace dawn::native
