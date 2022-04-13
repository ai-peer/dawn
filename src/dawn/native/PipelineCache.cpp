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

#include "dawn/native/PipelineCache.h"

namespace dawn::native {

    PipelineCacheBase::PipelineCacheBase(DeviceBase* device, BlobCache* cache, const CacheKey& key)
        : ObjectBase(device), mCache(cache), mKey(key) {
        mBlob = mCache->Load(mKey);
    }

    PipelineCacheBase::PipelineCacheBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : ObjectBase(device, tag) {
    }

    const CachedBlob* PipelineCacheBase::GetBlob() const {
        return &mBlob;
    }

    bool PipelineCacheBase::CacheHit() const {
        return !mBlob.Empty();
    }

    void PipelineCacheBase::Flush() {
        // Try to write the data out to the persistent cache.
        CachedBlob blob = SerializeToBlobImpl();
        if (blob.Size() > 0) {
            // Using a simple heuristic to decide whether to write out the blob right now. May need
            // smarter tracking when we are dealing with monolithic caches.
            mCache->Store(mKey, blob);
        }
    }

}  // namespace dawn::native
