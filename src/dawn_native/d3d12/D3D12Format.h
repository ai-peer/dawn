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

#ifndef DAWNNATIVE_D3D12_D3D12FORMAT_H_
#define DAWNNATIVE_D3D12_D3D12FORMAT_H_

#include "dawn_native/Format.h"

#include <dxgiformat.h>

namespace dawn_native { namespace d3d12 {

    class Device;

    struct D3D12Format {
        DXGI_FORMAT baseFormat = DXGI_FORMAT_UNKNOWN;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

        // Add uav/rtv/dsv formats as needed.
        DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;
    };

    using D3D12FormatTable = std::array<D3D12Format, kKnownFormatCount>;
    D3D12FormatTable BuildD3D12FormatTable();

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_D3D12FORMAT_H_
