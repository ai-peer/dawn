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

#ifndef SRC_DAWN_NATIVE_CACHERESULT_H_
#define SRC_DAWN_NATIVE_CACHERESULT_H_

#include <memory>
#include <utility>

#include "dawn/common/Assert.h"

namespace dawn::native {

template <typename T>
class CacheResult {
  public:
    static CacheResult CacheHit(CacheKey key, T value) {
        return CacheResult(std::move(key), std::move(value), true, nullptr);
    }

    static CacheResult CacheMiss(CacheKey key, T value, BlobCache* cache) {
        return CacheResult(std::move(key), std::move(value), false, cache);
    }

    CacheResult() : mKey(), mValue(), mIsCached(false), mIsValid(false) {}

    bool IsCached() const {
        ASSERT(mIsValid);
        return mIsCached;
    }
    const CacheKey& GetCacheKey() {
        ASSERT(mIsValid);
        return mKey;
    }

    // Note: Getting mValue is always const, since mutating it would invalidate consistency with
    // mKey.
    const T* operator->() const {
        ASSERT(mIsValid);
        return &mValue;
    }
    const T& operator*() const {
        ASSERT(mIsValid);
        return mValue;
    }

    // Types should define static void CacheResult<T>::Store to customize how T is serialized
    // into the BlobCache.
    static void Store(BlobCache* cache, const CacheKey& key, const T& value);

    void EnsureStored() {
        if (!IsCached() && mCache != nullptr) {
            Store(mCache, mKey, mValue);
            // After calling Store, the CacheResult is now cached.
            mIsCached = true;
        }
    }

    T Acquire() {
        ASSERT(mIsValid);
        mIsValid = false;
        return std::move(mValue);
    }

  private:
    CacheResult(CacheKey key, T value, bool isCached, BlobCache* cache)
        : mKey(std::move(key)),
          mValue(std::move(value)),
          mIsCached(isCached),
          mIsValid(true),
          mCache(cache) {}

    CacheKey mKey;
    T mValue;
    bool mIsCached;
    bool mIsValid;
    BlobCache* mCache;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_CACHERESULT_H_
