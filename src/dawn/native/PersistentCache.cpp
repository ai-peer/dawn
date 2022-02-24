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

#include "dawn/native/PersistentCache.h"

#include "dawn/common/Assert.h"
#include "dawn/native/Device.h"
#include "dawn/native/Instance.h"
#include "dawn/platform/DawnPlatform.h"

#include <sstream>

namespace dawn::native {

    // TODO(dawn:549): Create a fingerprint of concatenated version strings (ex. Dawn commit hash).
    // This will be used by the client so it may know when to discard previously cached Dawn
    // objects should this fingerprint change.
    PersistentCache::PersistentCache(InstanceBase* instance)
        : mCache(instance->GetPlatform()->GetCachingInterface(/*fingerprint*/ nullptr,
                                                              /*fingerprintSize*/ 0)) {
    }

    PersistentCache::Key PersistentCache::CreateKey(KeyType type,
                                                    const std::string& isolationKey,
                                                    const size_t hash) {
        std::stringstream stream;
        stream << static_cast<uint32_t>(type) << isolationKey << hash;
        return Key(std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{});
    }

    ScopedCachedBlob PersistentCache::LoadData(const Key& key) {
        std::lock_guard<std::mutex> lock(mMutex);
        return LoadDataInternal(key);
    }

    void PersistentCache::StoreData(const Key& key, const void* value, size_t size) {
        std::lock_guard<std::mutex> lock(mMutex);
        return StoreDataInternal(key, value, size);
    }

    void PersistentCache::StoreData(const Key& key, const ScopedCachedBlob& blob) {
        std::lock_guard<std::mutex> lock(mMutex);
        return StoreDataInternal(key, blob.buffer.get(), blob.bufferSize);
    }

    void PersistentCache::LoadAndUpdate(const Key& key, UpdateFn updateFn) {
        std::lock_guard<std::mutex> lock(mMutex);

        ScopedCachedBlob blob = LoadDataInternal(key);
        std::vector<uint8_t> result = updateFn(blob);
        StoreDataInternal(key, result.data(), result.size());
    }

    ScopedCachedBlob PersistentCache::LoadDataInternal(const Key& key) {
        if (mCache == nullptr) {
            return {};
        }

        ScopedCachedBlob blob = {};
        blob.bufferSize = mCache->LoadData(key.data(), key.size(), nullptr, 0);
        if (blob.bufferSize == 0) {
            return {};
        }

        blob.buffer.reset(new uint8_t[blob.bufferSize]);
        const size_t bufferSize =
            mCache->LoadData(key.data(), key.size(), blob.buffer.get(), blob.bufferSize);
        ASSERT(bufferSize == blob.bufferSize);
        return blob;
    }

    void PersistentCache::StoreDataInternal(const Key& key, const void* value, size_t size) {
        if (mCache == nullptr) {
            return;
        }
        ASSERT(value != nullptr);
        ASSERT(size > 0);
        std::lock_guard<std::mutex> lock(mMutex);
        mCache->StoreData(key.data(), key.size(), value, size);
    }

}  // namespace dawn::native
