// Copyright 2023 The Dawn Authors
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

#ifndef SRC_DAWN_COMMON_REFCOUNTEDOBJECTCACHE_H_
#define SRC_DAWN_COMMON_REFCOUNTEDOBJECTCACHE_H_

#include <unordered_set>

#include "dawn/common/Mutex.h"
#include "dawn/common/RefCounted.h"

// A cache to store ref counted objects and detect duplications.
// The cache won't retain any references to the objects. When an object has last ref dropped, it
// must be removed from the cache.
template <typename Object, typename HashFunc, typename EqualityFunc>
class RefCountedObjectCache {
  public:
    Ref<Object> Find(Object* key) {
        dawn::Mutex::AutoLock lock(&mTableMutex);

        return FindImpl(key);
    }

    std::pair<Ref<Object>, bool> Insert(Object* object) {
        dawn::Mutex::AutoLock lock(&mTableMutex);

        auto existingRef = FindImpl(object);
        if (existingRef != nullptr) {
            return {existingRef, /*inserted=*/false};
        }
        ASSERT(mTable.count(object) == 0);
        mTable.insert(object);
        return {object, /*inserted=*/true};
    }

    void Erase(Object* object) {
        dawn::Mutex::AutoLock lock(&mTableMutex);
        mTable.erase(object);
    }

    bool IsEmpty() const {
        dawn::Mutex::AutoLock lock(&mTableMutex);
        return mTable.empty();
    }

  private:
    Ref<Object> FindImpl(Object* key) {
        ASSERT(mTableMutex.IsLockedByCurrentThread());

        auto iter = mTable.find(key);
        if (iter != mTable.end()) {
            // We need to make sure that the object is not being deleted on another thread.
            // Need to check that ref count is not zero before we can add a reference.
            // Note: the object must call Erase() to remove itself from the cache before completing
            // its destruction.
            // There are 3 cases:
            // 1. object is still alive, ref count >= 1, normal case. TryReference() should
            // successfully add a reference to the object.
            // 2. object's last ref has dropped, but its destruction is in progress. Then
            // TryReference() should return false and no reference will be added to the object.
            // 3. object completes its destruction, then it must already be removed from the cache,
            // we shouldn't reach this point.
            if ((*iter)->TryReference()) {
                // No need to add another ref since already do so above. AcquireRef() will adopt the
                // pointer.
                return AcquireRef(*iter);
            }

            // Object is being destructed but hasn't called Erase() yet, erase it from the cache
            // here to allow another similar object to be inserted.
            mTable.erase(iter);
        }

        return nullptr;
    }

    std::unordered_set<Object*, HashFunc, EqualityFunc> mTable;
    mutable dawn::Mutex mTableMutex;
};

#endif
