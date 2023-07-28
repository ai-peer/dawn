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

#ifndef SRC_DAWN_NATIVE_SHAREDTEXTUREMEMORY_H_
#define SRC_DAWN_NATIVE_SHAREDTEXTUREMEMORY_H_

#include <vector>

#include "dawn/common/WeakRefSupport.h"
#include "dawn/native/Error.h"
#include "dawn/native/ObjectBase.h"
#include "dawn/native/SharedFence.h"
#include "dawn/native/dawn_platform.h"

namespace dawn::native {

struct SharedTextureMemoryDescriptor;
struct SharedTextureMemoryBeginAccessDescriptor;
struct SharedTextureMemoryEndAccessState;
struct SharedTextureMemoryProperties;
struct TextureDescriptor;

class SharedTextureMemoryBase : public ApiObjectBase,
                                public WeakRefSupport<SharedTextureMemoryBase> {
  public:
    using BeginAccessDescriptor = SharedTextureMemoryBeginAccessDescriptor;
    using EndAccessState = SharedTextureMemoryEndAccessState;

    static SharedTextureMemoryBase* MakeError(DeviceBase* device,
                                              const SharedTextureMemoryDescriptor* descriptor);

    void APIGetProperties(SharedTextureMemoryProperties* properties) const;
    TextureBase* APICreateTexture(const TextureDescriptor* descriptor);
    void APIBeginAccess(TextureBase* texture, const BeginAccessDescriptor* descriptor);
    void APIEndAccess(TextureBase* texture, EndAccessState* state);

    ObjectType GetType() const override;

    bool CheckCurrentAccess(const TextureBase* texture) const;

  protected:
    SharedTextureMemoryBase(DeviceBase* device,
                            const char* label,
                            const SharedTextureMemoryProperties& properties);
    void DestroyImpl() override;

    const SharedTextureMemoryProperties mProperties;

    Ref<TextureBase> mCurrentAccess;

  private:
    ResultOrError<Ref<TextureBase>> CreateTexture(const TextureDescriptor* descriptor);
    MaybeError BeginAccess(TextureBase* texture, const BeginAccessDescriptor* descriptor);
    MaybeError EndAccess(TextureBase* texture, EndAccessState* state);
    ResultOrError<FenceAndSignalValue> EndAccessInternal(TextureBase* texture,
                                                         EndAccessState* state);

    virtual ResultOrError<Ref<TextureBase>> CreateTextureImpl(const TextureDescriptor* descriptor);

    // BeginAccessImpl validates the operation in valid on the backend, and performs any
    // backend specific operations. It does NOT need to acquire begin fences; that is done in the
    // frontend in BeginAccess.
    virtual MaybeError BeginAccessImpl(TextureBase* texture,
                                       const BeginAccessDescriptor* descriptor);
    // EndAccessImpl validates the operation is valid on the backend, and returns the end fence.
    virtual ResultOrError<FenceAndSignalValue> EndAccessImpl(TextureBase* texture);

    SharedTextureMemoryBase(DeviceBase* device,
                            const SharedTextureMemoryDescriptor* descriptor,
                            ObjectBase::ErrorTag tag);
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_SHAREDTEXTUREMEMORY_H_
