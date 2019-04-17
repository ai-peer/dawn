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

#ifndef PLATFORM_WORKAROUNDS_H
#define PLATFORM_WORKAROUNDS_H

#include <bitset>

namespace dawn_native
{

    enum class Workarounds {
        // Emulate storing into multisampled color attachments and doing MSAA resolve
        // simultaneously. This workaround is needed in the following situations:
        // 1. On Metal drivers that do not support MTLStoreActionStoreAndMultisampleResolve, we
        // cannot do MSAA resolve and store valid rendering result into the multisampled color
        // attachments in the same time. To implement StoreOp::Store, we should do MSAA resolve in
        // another render pass after ending the previous one.
        // 2. On D3D12 drivers that do not support render pass, we can only do MSAA resolve with
        // ResolveSubresource().
        // Note that this flag is ignored on OpenGL backends because OpenGL drivers only support
        // doing MSAA resolve with glBlitFramebuffer().
        // Tracking issue: https://bugs.chromium.org/p/dawn/issues/detail?id=56
        EmulateStoreAndMSAAResolve = 0,

        WorkaroundsCount = 1
    };

    using WorkaroundsMask = std::bitset<static_cast<size_t>(Workarounds::WorkaroundsCount)>;

}  // namespace dawn_native

#endif
