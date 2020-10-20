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

#ifndef DAWNNATIVE_INTERNALPIPELINESTORE_H_
#define DAWNNATIVE_INTERNALPIPELINESTORE_H_

#include "dawn_native/ObjectBase.h"
#include "dawn_native/dawn_platform.h"
#include "dawn_native/internal_pipelines/InternalPipelineUtils.h"

#include <array>

namespace dawn_native {
    class DeviceBase;
    class RenderPipelineBase;
    class ShaderModuleBase;

    class InternalPipelineStore {
      public:
        InternalPipelineStore(DeviceBase* device);
        ~InternalPipelineStore();

        RenderPipelineBase* GetBlitTextureForBrowserPipeline(wgpu::TextureDimension srcDim,
                                                             wgpu::TextureFormat srcFormat,
                                                             wgpu::TextureDimension dstDim,
                                                             wgpu::TextureFormat dstFormat);

      private:
        DeviceBase* mDevice;
        std::array<RenderPipelineBase*, kInternalRenderPipelineCount> mInternalRenderPipelineCache;
        std::array<ShaderModuleBase*, kInternalShaderCount> mInternalShaderModuleCache;
    };
}  // namespace dawn_native

#endif  // DAWNNATIVE_INTERNALPIPELINESTORE_H_