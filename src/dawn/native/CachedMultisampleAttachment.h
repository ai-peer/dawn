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

#ifndef SRC_DAWN_NATIVE_CACHEDMULTISAMPLEATTACHMENT_H_
#define SRC_DAWN_NATIVE_CACHEDMULTISAMPLEATTACHMENT_H_

#include "dawn/common/RefCounted.h"
#include "dawn/native/CachedObject.h"
#include "dawn/native/ObjectBase.h"
#include "dawn/native/Texture.h"

namespace dawn::native {

class DeviceBase;

class CachedMultisampleAttachmentBlueprint {
  public:
    CachedMultisampleAttachmentBlueprint(wgpu::TextureFormat format,
                                         uint32_t width,
                                         uint32_t height,
                                         uint32_t sampleCount);
    explicit CachedMultisampleAttachmentBlueprint(const CachedMultisampleAttachmentBlueprint& rhs);

    // Functions necessary for the unordered_set<CachedMultisampleAttachment*>-based cache.
    struct HashFunc {
        size_t operator()(const CachedMultisampleAttachmentBlueprint* blueprint) const;
    };
    struct EqualityFunc {
        bool operator()(const CachedMultisampleAttachmentBlueprint* a,
                        const CachedMultisampleAttachmentBlueprint* b) const;
    };

  protected:
    wgpu::TextureFormat mFormat;
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mSampleCount;
};

// Class to hold a cached multisample texture, which is used as an implicit multisample attachment
// for a MSAA render to single sampled render pass. This object is ref counted since we want to
// deallocate the multisample texture after the last single sampled texture used it has been
// deallocated.
class CachedMultisampleAttachment final : public CachedMultisampleAttachmentBlueprint,
                                          public ObjectBase,
                                          public CachedObject {
  public:
    CachedMultisampleAttachment(DeviceBase* device,
                                const CachedMultisampleAttachmentBlueprint& blueprint);
    ~CachedMultisampleAttachment() override;

    MaybeError Initialize();

    TextureBase* GetTexture();
    TextureViewBase* GetTextureView();

    size_t ComputeContentHash() override;

  private:
    Ref<TextureBase> mTexture;
    Ref<TextureViewBase> mTextureView;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_CACHEDMULTISAMPLEATTACHMENT_H_
