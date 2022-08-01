// Copyright 2022 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_APPLYCLEARVALUEWITHDRAWHELPER_H_
#define SRC_DAWN_NATIVE_APPLYCLEARVALUEWITHDRAWHELPER_H_

#include <unordered_map>

#include "dawn/common/Constants.h"
#include "dawn/native/Error.h"

namespace dawn::native {

class BufferBase;
class RenderPassEncoder;
struct RenderPassDescriptor;

using KeyOfApplyClearColorWithDrawPipelines = std::array<wgpu::TextureFormat, kMaxColorAttachments>;

struct KeyOfApplyClearColorWithDrawPipelinesHashFunc {
    size_t operator()(KeyOfApplyClearColorWithDrawPipelines key) const;
};

struct KeyOfApplyClearColorWithDrawPipelinesEqualityFunc {
    bool operator()(KeyOfApplyClearColorWithDrawPipelines key1,
                    const KeyOfApplyClearColorWithDrawPipelines key2) const;
};

using ApplyClearColorWithDrawPipelinesCache =
    std::unordered_map<KeyOfApplyClearColorWithDrawPipelines,
                       Ref<RenderPipelineBase>,
                       KeyOfApplyClearColorWithDrawPipelinesHashFunc,
                       KeyOfApplyClearColorWithDrawPipelinesEqualityFunc>;

bool ShouldApplyClearValueWithDraw(DeviceBase* device,
                                   const RenderPassDescriptor* renderPassDescriptor);

ResultOrError<Ref<BufferBase>> CreateUniformBufferWithClearValues(
    DeviceBase* device,
    const RenderPassDescriptor* renderPassDescriptor);

MaybeError ApplyClearValueWithDraw(RenderPassEncoder* renderPassEncoder,
                                   Ref<BufferBase> uniformBufferWithClearData,
                                   const RenderPassDescriptor* renderPassDescriptor);

}  // namespace dawn::native

#endif
