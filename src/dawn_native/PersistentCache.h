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

#ifndef DAWNNATIVE_PERSISTENTCACHE_H_
#define DAWNNATIVE_PERSISTENTCACHE_H_

#include "dawn_platform/DawnPlatform.h"  // forward declare?

#include <array>

namespace dawn_native {

    static constexpr size_t kKeyLength = 20;  // TODO: Figure out this value.
    using PersistentCacheKey = std::array<uint8_t, kKeyLength>;

    class PersistentCache {
      public:
        PersistentCache(dawn_platform::Platform* platform);

        size_t loadData(PersistentCacheKey key, void* value, size_t size);
        void storeData(PersistentCacheKey key, const void* value, size_t size);

        size_t getDataSize(PersistentCacheKey key);

      protected:
        dawn_platform::Platform* mPlatform = nullptr;
    };
}  // namespace dawn_native

#endif  // DAWNNATIVE_PERSISTENTCACHE_H_