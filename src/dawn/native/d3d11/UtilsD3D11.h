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

#ifndef SRC_DAWN_NATIVE_D3D11_UTILSD3D11_H_
#define SRC_DAWN_NATIVE_D3D11_UTILSD3D11_H_

#include <string>

#include "dawn/native/Commands.h"
#include "dawn/native/d3d11/BufferD3D11.h"
// #include "dawn/native/d3d11/TextureCopySplitter.h"
#include "dawn/native/d3d11/TextureD3D11.h"
#include "dawn/native/d3d11/d3d11_platform.h"
#include "dawn/native/dawn_platform.h"

namespace dawn::native::d3d11 {

ResultOrError<std::wstring> ConvertStringToWstring(std::string_view s);

D3D11_COMPARISON_FUNC ToD3D11ComparisonFunc(wgpu::CompareFunction func);

// D3D11_TEXTURE_COPY_LOCATION ComputeTextureCopyLocationForTexture(const Texture* texture,
//                                                                  uint32_t level,
//                                                                  uint32_t layer,
//                                                                  Aspect aspect);

// D3D11_TEXTURE_COPY_LOCATION ComputeBufferLocationForCopyTextureRegion(
//     const Texture* texture,
//     ID3D11Resource* bufferResource,
//     const Extent3D& bufferSize,
//     const uint64_t offset,
//     const uint32_t rowPitch,
//     Aspect aspect);
// D3D11_BOX ComputeD3D11BoxFromOffsetAndSize(const Origin3D& offset, const Extent3D& copySize);

bool IsTypeless(DXGI_FORMAT format);

enum class BufferTextureCopyDirection {
    B2T,
    T2B,
};

// void SetDebugName(Device* device, ID3D11Object* object, const char* prefix, std::string label =
// "");

uint64_t MakeDXCVersion(uint64_t majorVersion, uint64_t minorVersion);

}  // namespace dawn::native::d3d11

#endif  // SRC_DAWN_NATIVE_D3D11_UTILSD3D11_H_
