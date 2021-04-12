// Copyright 2021 The Dawn Authors
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

#ifndef DAWNNATIVE_CHAIN_UTILS_H_
#define DAWNNATIVE_CHAIN_UTILS_H_

#include "dawn_native/dawn_platform.h"

namespace dawn_native {

    #define ForEachChain(__iter, __start) \
        for (ChainedStruct const * __iter = __start; __iter; __iter = __iter->nextInChain)

    // Returns the first ChainedStruct of type sType in the chain (or nullptr if no matching chain
    // could be found). Inspired by Mesa's Vulkan utilities for iterating over pNext chains.
    static inline ChainedStruct const * FindInChain(ChainedStruct const * start,
                                                    wgpu::SType sType) {
        ForEachChain(iter, start) {
            if (iter->sType == sType)
                return iter;
        }
        return nullptr;
    }

}  // namespace dawn_native

#endif  // DAWNNATIVE_CHAIN_UTILS_H_
