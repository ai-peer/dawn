// Copyright 2020 The Dawn Authors
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

#include "dawn_native/InternalPipelineStore.h"

#include "dawn_native/RenderPipeline.h"
#include "dawn_native/ShaderModule.h"

namespace dawn_native {
    InternalPipelineStore::~InternalPipelineStore() {
        // TODO(shaobo.yan@intel.com): Below code to release the resource doesn't work:
        // CopyTextureForBrowserFS.clear();
        // CopyTextureForBrowserPipeline.clear();
        // CopyTextureForBrowserVS = nullptr;

        for (std::pair<uint32_t, Ref<RenderPipelineBase>> element : CopyTextureForBrowserPipeline) {
            element.second.Get()->Release();
        }
        for (std::pair<uint32_t, Ref<ShaderModuleBase>> element : CopyTextureForBrowserFS) {
            element.second.Get()->Release();
        }
        CopyTextureForBrowserVS->Release();
    }
}  // namespace dawn_native