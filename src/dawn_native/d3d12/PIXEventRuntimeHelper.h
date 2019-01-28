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

#include "dawn_native/Error.h"
#include "dawn_native/d3d12/d3d12_platform.h"

#include <WinPixEventRuntime/pix3.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef USE_PIX
#    define USE_PIX
#endif

namespace dawn_native { namespace d3d12 {

    using PFN_PIX_GET_THREAD_INFO = PIXEventsThreadInfo*(WINAPI*)(void);
    PFN_PIX_GET_THREAD_INFO pixGetThreadInfo = nullptr;

    using PFN_PIX_EVENTS_REPLACE_BLOCK = UINT64(WINAPI*)(bool getEarliestTime);
    PFN_PIX_EVENTS_REPLACE_BLOCK pixEventsReplaceBlock = nullptr;

    // PIX expects us to link WinPixEventRuntime.dll at compile time. Instead,
    // we choose to dynamically load the .dll at startup. The external function
    // calls pix3.h expects to be linked at compile time instead will be linked
    // to these definitions that wrap the function addresses obtained at runtime.
    PIXEventsThreadInfo* PIXGetThreadInfo() {
        ASSERT(pixGetThreadInfo != nullptr);
        return pixGetThreadInfo();
    }

    UINT64 WINAPI PIXEventsReplaceBlock(bool getEarliestTime) {
        ASSERT(pixEventsReplaceBlock != nullptr);
        return pixEventsReplaceBlock(getEarliestTime);
    }
}}  // namespace dawn_native::d3d12

#ifdef __cplusplus
}
#endif