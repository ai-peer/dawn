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

#ifndef DAWNNATIVE_Fingerprint_RECORDER_H_
#define DAWNNATIVE_Fingerprint_RECORDER_H_

#include "common/HashUtils.h"

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
        virtual void Fingerprint(FingerprintRecorder* recorder) = 0;

        size_t GetKey() const;
        void SetKey(size_t key);

        // Functor necessary for the unordered_set<CachedObject*>-based cache.
        struct HashFunc {
            size_t operator()(const RecordedObject* obj) const;
        };

      private:
        friend class FingerprintRecorder;
        size_t mKey = kEmptyKeyValue;
    };

    // Visitor that builds a hash-based key that can be used to quickly lookup an object in a cache.
    class FingerprintRecorder {
      public:
        FingerprintRecorder() = default;

        template <typename T, typename... Args>
        void Record(const T& value, const Args&... args) {
            HashCombine(&mHash, value, args...);
        }

        template <typename IteratorT>
        void RecordIterable(const IteratorT& iterable) {
            for (auto it = iterable.begin(); it != iterable.end(); ++it) {
                Record(*it);
            }
        }

        // Called at the end of RecordedObject-based object construction.
        void RecordObject(RecordedObject* obj);

        size_t GetKey() const;

      private:
        size_t mHash = 0;
    };
}  // namespace dawn_native

#endif  // DAWNNATIVE_Fingerprint_RECORDER_H_