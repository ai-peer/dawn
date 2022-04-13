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

#ifndef SRC_DAWN_NATIVE_PIPELINECACHE_H_
#define SRC_DAWN_NATIVE_PIPELINECACHE_H_

#include "dawn/native/BlobCache.h"
#include "dawn/native/CacheKey.h"
#include "dawn/native/ObjectBase.h"

namespace dawn::native {

    // Abstraction layer for backend dependent pipeline caching. Note that while PipelineCacheBase
    // is an ObjectBase (and as a result takes a device), the device can be null in backend
    // implementations where a cache does not need to be directly associated with a single device.
    class PipelineCacheBase : public ObjectBase {
      public:
        // Returns the initial blob read from the cache at initialization.
        const CachedBlob* GetBlob() const;

        // Returns whether the pipeline cache was a hit or miss. Useful to determine whether we
        // should flush.
        bool CacheHit() const;

        // Serializes and writes the current contents of the backend cache object into the backing
        // blob cache.
        void Flush();

      protected:
        PipelineCacheBase(DeviceBase* device, BlobCache* cache, const CacheKey& key);

      private:
        PipelineCacheBase(DeviceBase* device, ObjectBase::ErrorTag tag);

        // Backend implementation of serialization of the cache into a blob. Note that errors in the
        // serialization are not surfaced, and an empty blob should be returned since caching should
        // be opaque to the user. Serializes the current state of the backend cache, hence may
        // return different results at different calls.
        virtual CachedBlob SerializeToBlobImpl() = 0;

        // The blob cache is owned by the Adapter and pipeline caches are owned/created by devices
        // or adapters. Since the device owns a reference to the Instance which owns the Adapter,
        // the blob cache is guaranteed to be valid throughout the lifetime of the object.
        BlobCache* mCache;
        CacheKey mKey;
        CachedBlob mBlob;
    };

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_PIPELINECACHE_H_
