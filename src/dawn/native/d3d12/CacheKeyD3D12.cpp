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

#include "dawn/native/d3d12/CacheKeyD3D12.h"

namespace dawn::native {

template <>
void CacheKeySerializer<ComPtr<ID3DBlob>>::Serialize(CacheKey* key, const ComPtr<ID3DBlob>& t) {
    // key->RecordIterable<char>(t->GetBufferPointer(), t->GetBufferSize());
    key->RecordIterable(reinterpret_cast<uint8_t*>(t->GetBufferPointer()), t->GetBufferSize());
}

template <>
void CacheKeySerializer<D3D12_DESCRIPTOR_RANGE>::Serialize(CacheKey* key,
                                                           const D3D12_DESCRIPTOR_RANGE& t) {
    // key->Record(t.BaseShaderRegister, t.RangeType, t.NumDescriptors, t.RegisterSpace);
    key->Record(t.RangeType, t.NumDescriptors, t.BaseShaderRegister, t.RegisterSpace,
                t.OffsetInDescriptorsFromTableStart);
}

template <>
void CacheKeySerializer<std::vector<D3D12_DESCRIPTOR_RANGE>>::Serialize(
    CacheKey* key,
    const std::vector<D3D12_DESCRIPTOR_RANGE>& t) {
    key->RecordIterable(t.data(), t.size());
    // vulkan::SerializePnext<>(key, &t);
}

}  // namespace dawn::native