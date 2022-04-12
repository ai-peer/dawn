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

#ifndef SRC_DAWN_NATIVE_PERSISTENTCACHE_H_
#define SRC_DAWN_NATIVE_PERSISTENTCACHE_H_

#include "dawn/common/RefCounted.h"
#include "dawn/native/Error.h"

#include <functional>
#include <mutex>
#include <vector>

namespace dawn::platform {
    class CachingInterface;
}

namespace dawn::native {

    class InstanceBase;

    struct ScopedCachedBlob {
        std::unique_ptr<uint8_t[]> buffer;
        size_t bufferSize = 0;
    };

    // The public API of this class must be thread-safe as they can be used in async functions.
    // Currently, the class synchronizes on all load/store operations, but we may be able to
    // optimize this more later to only block based on key.
    class PersistentCache {
      public:
        using Key = std::vector<uint8_t>;

        // Update functions should be able to handle `empty` blob inputs.
        using UpdateFn = std::function<std::vector<uint8_t>(const ScopedCachedBlob&)>;

        enum class KeyType { Pipeline };

        static Key CreateKey(KeyType type, const std::string& isolationKey, const size_t hash);

        explicit PersistentCache(InstanceBase* instance);

        // Load returns an empty blob result if the key is not found in the cache.
        ScopedCachedBlob LoadData(const Key& key);
        void StoreData(const Key& key, const void* value, size_t size);
        void StoreData(const Key& key, const ScopedCachedBlob& blob);

        // Atomically loads from key, uses the loaded value, and updates it according to `updateFn`.
        // Useful when the cache is a more monolithic-like cache where we can update it.
        void LoadAndUpdate(const Key& key, UpdateFn updateFn);

      private:
        // Non-thread safe internal implementations of load/store. Exposed callers that use these
        // helpers need to make sure that the functions are entered with `mMutex` held.
        ScopedCachedBlob LoadDataInternal(const Key& key);
        void StoreDataInternal(const Key& key, const void* value, size_t size);

        // Protects thread safety of access to mCache.
        std::mutex mMutex;
        dawn::platform::CachingInterface* mCache = nullptr;
    };
}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_PERSISTENTCACHE_H_
