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

#ifndef COMMON_ZEROREDSTRUCT_H_
#define COMMON_ZEROREDSTRUCT_H_

#include <cstring>

// Wrapper for structs to zero out its entirely at initialization for consistent memcpy results.
// Note that this should only be used for structs with only primitive members. Zero-ing out
// non-primitive members can lead to undefined behavior.
template <typename Struct>
struct ZeroedStruct : public Struct {
  public:
    ZeroedStruct() {
        memset(this, 0, sizeof(Struct));
    }
};

#endif  // COMMON_ZEROREDSTRUCT_H_
