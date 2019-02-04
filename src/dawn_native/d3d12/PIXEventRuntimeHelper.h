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

#ifndef DAWNNATIVE_D3D12_PIX_EVENT_RUNTIME_HELPER_H_
#define DAWNNATIVE_D3D12_PIX_EVENT_RUNTIME_HELPER_H_

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

    using PFN_PIX_EVENTS_REPLACE_BLOCK = UINT64(WINAPI*)(bool getEarliestTime);

    PIXEventsThreadInfo* PIXGetThreadInfo();
    UINT64 PIXEventsReplaceBlock(bool getEarliestTime);

}}  // namespace dawn_native::d3d12

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DAWNNATIVE_D3D12_PIX_EVENT_RUNTIME_HELPER_H_