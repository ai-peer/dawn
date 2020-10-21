// Copyright 2019 The Dawn Authors
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

#ifndef DAWNNATIVE_CACHED_OBJECT_H_
#define DAWNNATIVE_CACHED_OBJECT_H_

#include "dawn_native/ObjectBase.h"

#include <limits>

namespace dawn_native {

    class FingerprintRecorder;

    static constexpr size_t kEmptyKeyValue = std::numeric_limits<size_t>::max();

    // Object that knows how to record itself upon creation so it may be used as a cache key.
    // This interface is separate from CachedObject because some cached objects are never cached
    // and only used for lookup.
    class RecordedObject {
      public:
        // Called upon creation to record the objects immutable state.
        // Once recorded, getKey() can be used to lookup or compare the object.
        virtual void Fingerprint(FingerprintRecorder* recorder) = 0;

        size_t getKey() const;

        // Functors necessary for the unordered_set<CachedObject*>-based cache.
        struct HashFunc {
            size_t operator()(const RecordedObject* obj) const;
        };

        struct EqualityFunc {
            bool operator()(const RecordedObject* a, const RecordedObject* b) const;
        };

      private:
        friend class FingerprintRecorder;
        size_t mKey = kEmptyKeyValue;
    };

    // Some objects are cached so that instead of creating new duplicate objects,
    // we increase the refcount of an existing object.
    // When an object is successfully created, the device should call
    // SetIsCachedReference() and insert the object into the cache.
    class CachedObject : public ObjectBase {
      public:
        using ObjectBase::ObjectBase;

        bool IsCachedReference() const;

      private:
        friend class DeviceBase;
        void SetIsCachedReference();

        bool mIsCachedReference = false;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_CACHED_OBJECT_H_
