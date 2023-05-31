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

#include "dawn/native/CachedMultisampleAttachment.h"

#include "dawn/native/Device.h"
#include "dawn/native/ObjectContentHasher.h"

namespace dawn::native {
CachedMultisampleAttachmentBlueprint::CachedMultisampleAttachmentBlueprint(
    wgpu::TextureFormat format,
    uint32_t width,
    uint32_t height,
    uint32_t sampleCount)
    : mFormat(format), mWidth(width), mHeight(height), mSampleCount(sampleCount) {}

CachedMultisampleAttachmentBlueprint::CachedMultisampleAttachmentBlueprint(
    const CachedMultisampleAttachmentBlueprint& rhs) = default;

size_t CachedMultisampleAttachmentBlueprint::HashFunc::operator()(
    const CachedMultisampleAttachmentBlueprint* blueprint) const {
    ObjectContentHasher recorder;
    recorder.Record(blueprint->mFormat, blueprint->mWidth, blueprint->mHeight,
                    blueprint->mSampleCount);

    return recorder.GetContentHash();
}

bool CachedMultisampleAttachmentBlueprint::EqualityFunc::operator()(
    const CachedMultisampleAttachmentBlueprint* a,
    const CachedMultisampleAttachmentBlueprint* b) const {
    return a->mFormat == b->mFormat && a->mWidth == b->mWidth && a->mHeight == b->mHeight &&
           a->mSampleCount == b->mSampleCount;
}

CachedMultisampleAttachment::CachedMultisampleAttachment(
    DeviceBase* device,
    const CachedMultisampleAttachmentBlueprint& blueprint)
    : CachedMultisampleAttachmentBlueprint(blueprint), ObjectBase(device) {}

CachedMultisampleAttachment::~CachedMultisampleAttachment() {
    if (IsCachedReference()) {
        // Do not uncache the actual cached object if we are a blueprint.
        GetDevice()->UncacheMultisampleAttachment(this);
    }
}

size_t CachedMultisampleAttachment::ComputeContentHash() {
    return CachedMultisampleAttachmentBlueprint::HashFunc()(this);
}

MaybeError CachedMultisampleAttachment::Initialize() {
    TextureDescriptor desc = {};
    desc.dimension = wgpu::TextureDimension::e2D;
    desc.format = mFormat;
    desc.size = {mWidth, mHeight, 1};
    desc.sampleCount = mSampleCount;
    desc.usage = wgpu::TextureUsage::RenderAttachment;
    if (GetDevice()->HasFeature(Feature::TransientAttachments)) {
        desc.usage = desc.usage | wgpu::TextureUsage::TransientAttachment;
    }

    DAWN_TRY_ASSIGN(mTexture, GetDevice()->CreateTexture(&desc));

    DAWN_TRY_ASSIGN(mTextureView, mTexture->CreateView());

    return {};
}

TextureBase* CachedMultisampleAttachment::GetTexture() {
    ASSERT(mTexture != nullptr);
    return mTexture.Get();
}

TextureViewBase* CachedMultisampleAttachment::GetTextureView() {
    ASSERT(mTextureView != nullptr);
    return mTextureView.Get();
}

}  // namespace dawn::native
