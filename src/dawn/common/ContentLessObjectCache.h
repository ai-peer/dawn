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

#ifndef SRC_DAWN_COMMON_CONTENTLESSOBJECTCACHE_H_
#define SRC_DAWN_COMMON_CONTENTLESSOBJECTCACHE_H_

#include <mutex>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>

#include "dawn/common/Log.h"
#include "dawn/common/Ref.h"
#include "dawn/common/RefCounted.h"
#include "dawn/common/WeakRef.h"
#include "dawn/common/WeakRefSupport.h"

namespace dawn {

template <typename RefCountedT>
class ContentLessObjectCache;

namespace detail {

// The cache always holds WeakRefs internally, however, to enable lookups using pointers, we use a
// variant type so that the Hash and Equality functors can be handled properly.
template <typename RefCountedT>
using ContentLessObjectCacheKey = std::variant<RefCountedT*, WeakRef<RefCountedT>>;

template <typename RefCountedT>
struct ContentLessObjectCacheHashVisitor {
    using BaseHashFunc = RefCountedT::HashFunc;

    size_t operator()(const RefCountedT* ptr) const {
        DAWN_DEBUG() << "Pointer hash called and returned: " << BaseHashFunc()(ptr);
        return BaseHashFunc()(ptr);
    }
    size_t operator()(const WeakRef<RefCountedT>& weakref) const {
        Ref<RefCountedT> ref = weakref.Promote();
        if (ref.Get() == nullptr) {
            return 0;
        }
        DAWN_DEBUG() << "WeakRef hash called and returned: " << BaseHashFunc()(ref.Get());
        return BaseHashFunc()(ref.Get());
    }
};

template <typename RefCountedT>
struct ContentLessObjectCacheKeyFuncs {
    struct HashFunc {
        size_t operator()(const ContentLessObjectCacheKey<RefCountedT>& key) const {
            return std::visit(ContentLessObjectCacheHashVisitor<RefCountedT>(), key);
        }
    };

    using BaseEqualityFunc = RefCountedT::EqualityFunc;
    struct EqualityFunc {
        bool operator()(const ContentLessObjectCacheKey<RefCountedT>& a,
                        const ContentLessObjectCacheKey<RefCountedT>& b) const {
            RefCountedT* aPtr = nullptr;
            RefCountedT* bPtr = nullptr;

            Ref<RefCountedT> aRef;
            if (std::holds_alternative<RefCountedT*>(a)) {
                aPtr = std::get<RefCountedT*>(a);
                DAWN_DEBUG() << "A raw pointer: " << aPtr;
            } else {
                WeakRef<RefCountedT> weakref = std::get<WeakRef<RefCountedT>>(a);
                aRef = std::get<WeakRef<RefCountedT>>(a).Promote();
                DAWN_DEBUG() << "A ref pointer: " << aRef.Get();
                aPtr = aRef.Get();
            }
            if (aPtr == nullptr) {
                DAWN_DEBUG() << "A was nullptr";
                // return false;
            }

            Ref<RefCountedT> bRef;
            if (std::holds_alternative<RefCountedT*>(b)) {
                bPtr = std::get<RefCountedT*>(b);
                DAWN_DEBUG() << "B raw pointer: " << bPtr;
            } else {
                bRef = std::get<WeakRef<RefCountedT>>(b).Promote();
                DAWN_DEBUG() << "B ref pointer: " << bRef.Get();
                bPtr = bRef.Get();
            }
            if (bPtr == nullptr) {
                DAWN_DEBUG() << "B was nullptr";
                // return false;
            }

            DAWN_DEBUG() << "A: " << aPtr << " B: " << bPtr;

            if (aPtr == nullptr || bPtr == nullptr) {
                return false;
            }

            return BaseEqualityFunc()(aPtr, bPtr);
        }
    };
};

}  // namespace detail

// Classes need to extend this type if they want to be cacheable via the ContentLessObjectCache.
template <typename RefCountedT>
class ContentLessObjectCacheable : public WeakRefSupport<RefCountedT> {
  protected:
    ~ContentLessObjectCacheable() override {
        // TODO(lokokung) This might be unsafe because Erase calls into the EqualityFunc of
        // RefCountedT which is gone by the time this dtor runs assuming RefCountedT extends
        // ContentLessObjectCacheable<RefCountedT>.
        Uncache();
    }

    void Uncache() {
        Ref<ContentLessObjectCache<RefCountedT>> cache = mCache.Promote();
        if (cache.Get() != nullptr) {
            cache->Erase(static_cast<RefCountedT*>(this));
        }
        mCache = nullptr;
    }

  private:
    friend class ContentLessObjectCache<RefCountedT>;

    // WeakRef to the owning cache if we were inserted at any point.
    WeakRef<ContentLessObjectCache<RefCountedT>> mCache = nullptr;
};

// The ContentLessObjectCache stores raw pointers to living Refs without adding to their refcounts.
// This means that any RefCountedT that is inserted into the cache needs to make sure that their
// DeleteThis function erases itself from the cache. Otherwise, the cache can grow indefinitely via
// leaked pointers to deleted Refs.
template <typename RefCountedT>
class ContentLessObjectCache : public RefCounted,
                               public WeakRefSupport<ContentLessObjectCache<RefCountedT>> {
  public:
    // The dtor asserts that the cache is empty to aid in finding pointer leaks that can be possible
    // if the RefCountedT doesn't correctly implement the DeleteThis function to erase itself from
    // the cache.
    ~ContentLessObjectCache() override { ASSERT(Empty()); }

    // Inserts the object into the cache returning a pair where the first is a Ref to the inserted
    // or existing object, and the second is a bool that is true if we inserted `object` and false
    // otherwise.
    std::pair<Ref<RefCountedT>, bool> Insert(Ref<RefCountedT> obj) {
        DAWN_DEBUG() << "Inserting: " << obj.Get();
        std::lock_guard<std::mutex> lock(mMutex);
        WeakRef<RefCountedT> weakref = obj.GetWeakRef();
        auto [it, inserted] = mCache.insert(weakref);
        if (inserted) {
            DAWN_DEBUG() << "Inserted!";
            obj->mCache = this;
            DAWN_DEBUG() << "Inserted objects pointer: "
                         << std::get<WeakRef<RefCountedT>>(*it).Promote().Get();
            return {obj, inserted};
        } else {
            // Try to promote the found WeakRef to a Ref. If promotion fails, remove the old Key and
            // insert this one.
            Ref<RefCountedT> ref = std::get<WeakRef<RefCountedT>>(*it).Promote();
            if (ref != nullptr) {
                return {ref, false};
            } else {
                mCache.erase(it);
                auto result = mCache.insert(weakref);
                obj->mCache = this;
                ASSERT(result.second);
                return {obj, true};
            }
        }
    }

    // Returns a valid Ref<T> if the underlying we are able to Promote the WeakRef. Otherwise,
    // returns nullptr.
    Ref<RefCountedT> Find(RefCountedT* blueprint) {
        std::lock_guard<std::mutex> lock(mMutex);
        auto it = mCache.find(blueprint);
        if (it != mCache.end()) {
            return std::get<WeakRef<RefCountedT>>(*it).Promote();
        }
        return nullptr;
    }

    // Erases the object from the cache if it exists and are pointer equal. Otherwise does not
    // modify the cache.
    void Erase(RefCountedT* object) {
        std::lock_guard<std::mutex> lock(mMutex);
        DAWN_DEBUG() << "Erasing: " << object;
        auto it = mCache.find(object);
        if (it == mCache.end()) {
            return;
        }
        DAWN_DEBUG() << "Found something to erase";
        Ref<RefCountedT> ref = std::get<WeakRef<RefCountedT>>(*it).Promote();
        DAWN_DEBUG() << "Found: " << ref.Get();
        if (ref.Get() == object) {
            DAWN_DEBUG() << "Erased!";
            mCache.erase(it);
        }
    }

    // Returns true iff the cache is empty.
    bool Empty() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mCache.empty();
    }

  private:
    std::mutex mMutex;
    std::unordered_set<detail::ContentLessObjectCacheKey<RefCountedT>,
                       typename detail::ContentLessObjectCacheKeyFuncs<RefCountedT>::HashFunc,
                       typename detail::ContentLessObjectCacheKeyFuncs<RefCountedT>::EqualityFunc>
        mCache;
};

}  // namespace dawn

#endif  // SRC_DAWN_COMMON_CONTENTLESSOBJECTCACHE_H_
