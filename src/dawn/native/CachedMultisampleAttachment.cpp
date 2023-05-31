// Copyright 2019 The Dawn Authors
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
    : mFormat(format), mWidth(width), mHeight(height), mSampleCount(sampleCount) {
    SetContentHash(ComputeContentHash());
}

CachedMultisampleAttachmentBlueprint::CachedMultisampleAttachmentBlueprint(
    const CachedMultisampleAttachmentBlueprint& rhs) = default;

size_t CachedMultisampleAttachmentBlueprint::ComputeContentHash() {
    ObjectContentHasher recorder;
    recorder.Record(mFormat, mWidth, mHeight, mSampleCount);

    return recorder.GetContentHash();
}

bool CachedMultisampleAttachmentBlueprint::IsEqual(
    const CachedMultisampleAttachmentBlueprint* rhs) const {
    return mFormat == rhs->mFormat && mWidth == rhs->mWidth && mHeight == rhs->mHeight &&
           mSampleCount == rhs->mSampleCount;
}

bool CachedMultisampleAttachmentBlueprint::EqualityFunc::operator()(
    const CachedMultisampleAttachmentBlueprint* a,
    const CachedMultisampleAttachmentBlueprint* b) const {
    return a->IsEqual(b);
}

CachedMultisampleAttachment::CachedMultisampleAttachment(
    DeviceBase* device,
    const CachedMultisampleAttachmentBlueprint& blueprint)
    : CachedMultisampleAttachmentBlueprint(blueprint), mDevice(device) {}

CachedMultisampleAttachment::~CachedMultisampleAttachment() {
    if (IsCachedReference()) {
        // Do not uncache the actual cached object if we are a blueprint.
        mDevice->UncacheMultisampleAttachment(this);
    }
}

MaybeError CachedMultisampleAttachment::Initialize() {
    TextureDescriptor desc = {};
    desc.dimension = wgpu::TextureDimension::e2D;
    desc.format = mFormat;
    desc.size = {mWidth, mHeight, 1};
    desc.sampleCount = mSampleCount;
    desc.usage = wgpu::TextureUsage::RenderAttachment;
    if (mDevice->HasFeature(Feature::TransientAttachments)) {
        desc.usage = desc.usage | wgpu::TextureUsage::TransientAttachment;
    }

    DAWN_TRY_ASSIGN(mTexture, mDevice->CreateTexture(&desc));

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
