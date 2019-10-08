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
    const char* HResultAsString(HRESULT result) {
        switch (result) {
            case 0x00000000:
                return "S_OK";
            case 0x80004001:
                return "E_NOTIMPL";
            case 0x80004002:
                return "E_NOINTERFACE";
            case 0x80004003:
                return "E_POINTER";
            case 0x80004004:
                return "E_ABORT";
            case 0x80004005:
                return "E_FAIL";
            case 0x8000FFFF:
                return "E_UNEXPECTED";
            case 0x80070005:
                return "E_ACCESSDENIED";
            case 0x80070006:
                return "E_HANDLE";
            case 0x8007000E:
                return "E_OUTOFMEMORY";
            case 0x80070057:
                return "E_INVALIDARG";
            default:
                return "<Unknown HResult>";
        }
    }

    MaybeError CheckHRESULT(HRESULT result, const char* context) {
        if (SUCCEEDED(result)) {
            return {};
        }

        std::string message = std::string(context) + " failed with " + HResultAsString(result);
        return DAWN_DEVICE_LOST_ERROR(message);
    }

}}  // namespace dawn_native::d3d12