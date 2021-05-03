// Copyright 2018 The Dawn Authors
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

#ifndef DAWNNATIVE_PASSRESOURCEUSAGE_H
#define DAWNNATIVE_PASSRESOURCEUSAGE_H

#include "dawn_native/SubresourceStorage.h"
#include "dawn_native/dawn_platform.h"

#include <set>
#include <vector>

namespace dawn_native {

    class BufferBase;
    class QuerySetBase;
    class TextureBase;

    enum class PassType { Render, Compute };

    // The texture usage inside passes must be tracked per-subresource.
    using TextureSubresourceUsage = SubresourceStorage<wgpu::TextureUsage>;

    // Which resources are used by a synchronization scope and how they are used. The command
    // buffer validation pre-computes this information so that backends with explicit barriers
    // don't have to re-compute it.
    struct SyncScopeResourceUsage {
        std::vector<BufferBase*> buffers;
        std::vector<wgpu::BufferUsage> bufferUsages;

        std::vector<TextureBase*> textures;
        std::vector<TextureSubresourceUsage> textureUsages;
    };

    struct ComputePassResourceUsage : public SyncScopeResourceUsage {};

    struct RenderPassResourceUsage : public SyncScopeResourceUsage {
        // Storage to track the occlusion queries used during the pass.
        std::vector<QuerySetBase*> querySets;
        std::vector<std::vector<bool>> queryAvailabilities;
    };

    using RenderPassUsages = std::vector<RenderPassResourceUsage>;
    using ComputePassUsages = std::vector<ComputePassResourceUsage>;

    struct CommandBufferResourceUsage {
        RenderPassUsages renderPasses;
        ComputePassUsages computePasses;
        std::set<BufferBase*> topLevelBuffers;
        std::set<TextureBase*> topLevelTextures;
        std::set<QuerySetBase*> usedQuerySets;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_PASSRESOURCEUSAGE_H
