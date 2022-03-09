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

#include "dawn/native/CacheKeySerializer.h"

namespace dawn::native {

    void CacheKeySerialize(CacheKey* key, const std::string& t) {
        key->push_back('"');
        key->insert(key->end(), t.begin(), t.end());
        key->push_back('"');
    }

    void CacheKeySerialize(CacheKey* key, const CacheKey& t) {
        key->insert(key->end(), t.begin(), t.end());
    }

}  // namespace dawn::native
