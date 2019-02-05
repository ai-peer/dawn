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

#include "PIXEventRuntimeHelper.h"
#include "dawn_native/Error.h"

namespace dawn_native { namespace d3d12 {

    PFN_PIX_GET_THREAD_INFO pixGetThreadInfo = nullptr;
    PFN_PIX_EVENTS_REPLACE_BLOCK pixEventsReplaceBlock = nullptr;

    // PIX expects us to link WinPixEventRuntime.dll at compile time. Instead, we choose to
    // dynamically load the .dll at startup. The external function calls pix3.h expects to be linked
    // at compile time instead will be linked to these definitions that wrap the function addresses
    // obtained at runtime.
    PIXEventsThreadInfo* PIXGetThreadInfo() {
        ASSERT(pixGetThreadInfo != nullptr);
        return pixGetThreadInfo();
    }

    UINT64 PIXEventsReplaceBlock(bool getEarliestTime) {
        ASSERT(pixEventsReplaceBlock != nullptr);
        return pixEventsReplaceBlock(getEarliestTime);
    }
}}  // namespace dawn_native::d3d12