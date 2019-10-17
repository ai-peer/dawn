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

#include "dawn_native/d3d12/D3D12Error.h"

#include <string>

namespace dawn_native { namespace d3d12 {
    const char* HRESULTAsString(HRESULT result) {
        switch (result) {
            case S_OK:
                return "S_OK";
            case S_FALSE:
                return "S_FALSE";
            case E_FAIL:
                return "E_FAIL";
            case E_OUTOFMEMORY:
                return "E_OUTOFMEMORY";
            case E_INVALIDARG:
                return "E_INVALIDARG";
            default:
                return "<Unknown HRESULT>";
        }
    }

    MaybeError CheckHRESULT(HRESULT result, const char* context) {
        if (DAWN_LIKELY(SUCCEEDED(result))) {
            return {};
        }

        std::string message = std::string(context) + " failed with " + HRESULTAsString(result);

        switch (result) {
            case E_OUTOFMEMORY:
                return DAWN_OUT_OF_MEMORY_ERROR(message);
            default:
                return DAWN_DEVICE_LOST_ERROR(message);
        }
    }

}}  // namespace dawn_native::d3d12