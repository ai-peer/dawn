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

#ifndef SRC_DAWN_NATIVE_VULKAN_IOSURFACEUTILS_H_
#define SRC_DAWN_NATIVE_VULKAN_IOSURFACEUTILS_H_

#include <IOSurface/IOSurface.h>

#include "dawn/native/Error.h"

namespace dawn::native {
struct TextureDescriptor;
namespace vulkan {
class Texture;

MaybeError ValidateIOSurfaceCanBeImported(const TextureDescriptor* descriptor,
                                          IOSurfaceRef iosurface);

MaybeError ImportFromIOSurfaceToTexture(IOSurfaceRef iosurface, Texture* texture);
MaybeError ExportFromTextureToIOSurface(Texture* texture, IOSurfaceRef iosurface);

}  // namespace vulkan
}  // namespace dawn::native

#endif
