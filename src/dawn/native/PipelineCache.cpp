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

#include "dawn/native/Adapter.h"
#include "dawn/native/Device.h"
#include "dawn/native/PersistentCache.h"

namespace dawn::native {

    PipelineCacheBase::PipelineCacheBase(DeviceBase* device,
                                         PersistentCache* cache,
                                         PersistentCache::Key&& key)
        : ObjectBase(device), mCache(device->GetAdapter()->GetPersistentCache()), mKey(key) {
    }

    const ScopedCachedBlob* PipelineCacheBase::GetBlob() const {
        return &mBlob;
    }

    ScopedCachedBlob PipelineCacheBase::SerializeToBlob() {
        return SerializeToBlobImpl();
    }

    bool PipelineCacheBase::Initialize() {
        mBlob = mCache->LoadData(mKey);
        return true;
    }

    void PipelineCacheBase::DeleteThis() {
        // Try to write the data out to the persistent cache.
        ScopedCachedBlob blob = SerializeToBlob();
        if (blob.bufferSize > 0 && blob.bufferSize != mBlob.bufferSize) {
            // Using a simple heuristic to decide whether to write out the blob right now. May need
            // smarter tracking when we are dealing with monolithic caches.
            mCache->StoreData(mKey, blob);
        }
    }

}  // namespace dawn::native
