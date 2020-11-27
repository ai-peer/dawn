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

#include "dawn_native/PassResourceUsageTracker.h"

#include "dawn_native/Buffer.h"
#include "dawn_native/EnumMaskIterator.h"
#include "dawn_native/Format.h"
#include "dawn_native/Texture.h"

#include <utility>

namespace dawn_native {
    PassResourceUsageTracker::PassResourceUsageTracker(PassType passType) : mPassType(passType) {
    }

    void PassResourceUsageTracker::BufferUsedAs(BufferBase* buffer, wgpu::BufferUsage usage) {
        // std::map's operator[] will create the key and return 0 if the key didn't exist
        // before.
        mBufferUsages[buffer] |= usage;
    }

    void PassResourceUsageTracker::TextureViewUsedAs(TextureViewBase* view,
                                                     wgpu::TextureUsage usage) {
        TextureBase* texture = view->GetTexture();
        const SubresourceRange& range = view->GetSubresourceRange();

        // std::map's operator[] will create the key and return a PassTextureUsage with usage = 0
        // and an empty vector for subresourceUsages.
        // TODO (yunchao.he@intel.com): optimize this
        auto it = mTextureUsages.emplace(texture, texture);
        PassTextureUsage& textureUsage = it.first->second;

        // Set parameters for the whole texture
        textureUsage.usage |= usage;

        textureUsage.subresourceUsages.Update(
            range,
            [usage](const SubresourceRange&, const wgpu::TextureUsage& usage_)
                -> wgpu::TextureUsage { return usage_ | usage; });
    }

    void PassResourceUsageTracker::AddTextureUsage(TextureBase* texture,
                                                   const PassTextureUsage& textureUsage) {
        auto it = mTextureUsages.emplace(texture, texture);
        PassTextureUsage& passTextureUsage = it.first->second;

        passTextureUsage.usage |= textureUsage.usage;

        passTextureUsage.subresourceUsages.Merge<wgpu::TextureUsage>(
            textureUsage.subresourceUsages,
            [](const SubresourceRange&, const wgpu::TextureUsage& a,
               const wgpu::TextureUsage& b) -> wgpu::TextureUsage { return a | b; });
    }

    // Returns the per-pass usage for use by backends for APIs with explicit barriers.
    PassResourceUsage PassResourceUsageTracker::AcquireResourceUsage() {
        PassResourceUsage result;
        result.passType = mPassType;
        result.buffers.reserve(mBufferUsages.size());
        result.bufferUsages.reserve(mBufferUsages.size());
        result.textures.reserve(mTextureUsages.size());
        result.textureUsages.reserve(mTextureUsages.size());

        for (auto& it : mBufferUsages) {
            result.buffers.push_back(it.first);
            result.bufferUsages.push_back(it.second);
        }

        for (auto& it : mTextureUsages) {
            result.textures.push_back(it.first);
            result.textureUsages.push_back(std::move(it.second));
        }

        mBufferUsages.clear();
        mTextureUsages.clear();

        return result;
    }

}  // namespace dawn_native
