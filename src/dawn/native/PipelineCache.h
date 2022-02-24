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

#ifndef DAWNNATIVE_PIPELINECACHE_H_
#define DAWNNATIVE_PIPELINECACHE_H_

#include "dawn/native/ObjectBase.h"
#include "dawn/native/PersistentCache.h"

namespace dawn::native {
    // Abstraction layer for backend dependent pipeline caching.
    //
    // TODO(dawn:549) Known issues to solve (may be blocked on dependencies or just unfinished):
    //   - [Blocked] Currently does a blocking read-write operation to initialize pipelines because
    //     the read/write need to happen as atomically as possible to make sure we don't dirty the
    //     persistent cache given the current multi-adapter, multi-device use case.
    //   - [WIP] Currently, the implementation uses the isolation key to get a semi-monolithic
    //     cache. Since we don't have a TTL on the cache at the moment, the cache can grow until
    //     a new version of Chrome/Dawn is released. This will be solved via a two cache sliding
    //     window solution.
    class PipelineCacheBase : public ObjectBase {
      public:
        const ScopedCachedBlob* GetBlob() const;
        bool IsDirty() const;

        ScopedCachedBlob SerializeToBlob();

      protected:
        PipelineCacheBase(DeviceBase* device, PersistentCache* cache, PersistentCache::Key&& key);

        // Returns true iff we were successfully able to initialize the pipeline cache. This
        // includes loading the data from the cache and any backend initialization required. Note
        // that this initialization does not return an error because cache errors can be ignored and
        // normal initialization can just be executed.
        virtual bool Initialize();
        void DeleteThis() override;

        // Toggles dirty bit to determine if we need to write out to the persistent cache.
        void SetDirtied();

      private:
        // Backend implementation of serialization of the cache into a blob. Note that errors in the
        // serialization are not surfaced, and an empty blob should be returned since caching should
        // be opaque to the user.
        virtual ScopedCachedBlob SerializeToBlobImpl() = 0;

        // The persistent cache is owned by the Adapter and pipeline caches are owned by the
        // device. Since the device owns a reference to the Instance which owns the Adapter,
        // the persistent cache is guaranteed to be valid throughout the lifetime.
        PersistentCache* mCache;
        PersistentCache::Key mKey;

        bool mIsDirty = false;
        ScopedCachedBlob mBlob;
    };

}  // namespace dawn::native

#endif  // DAWNNATIVE_PIPELINECACHE_H_
