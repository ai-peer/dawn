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

#include "dawn_native/PersistentCache.h"

#include "dawn_platform/DawnPlatform.h"  // forward declare?

#include "common/Assert.h"

namespace dawn_native {

    PersistentCache::PersistentCache(dawn_platform::Platform* platform) : mPlatform(platform) {
    }

    size_t PersistentCache::loadData(PersistentCacheKey key, void* value, size_t size) {
        if (mPlatform == nullptr) {
            return 0;
        }
        return mPlatform->loadData(key.data(), key.size(), value, size);
    }

    bool PersistentCache::storeData(PersistentCacheKey key, const void* value, size_t size) {
        if (mPlatform == nullptr) {
            return false;
        }
        ASSERT(value != nullptr);
        ASSERT(size > 0);
        return mPlatform->storeData(key.data(), key.size(), value, size);
    }

    size_t PersistentCache::getDataSize(PersistentCacheKey key) {
        return mPlatform->loadData(key.data(), key.size(), nullptr, 0);
    }

    // static
    PersistentCacheKey PersistentCache::CreateKey(size_t hash) {
        PersistentCacheKey key = {};
        ASSERT(sizeof(hash) < key.size());
        memcpy(key.data(), &hash, sizeof(hash));
        return key;
    }

}  // namespace dawn_native