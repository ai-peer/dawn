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

#ifndef SRC_DAWN_NATIVE_CACHEREQUEST_H_
#define SRC_DAWN_NATIVE_CACHEREQUEST_H_

#include <memory>
#include <utility>

#include "dawn/common/Assert.h"
#include "dawn/common/Compiler.h"
#include "dawn/native/CacheKey.h"
#include "dawn/native/CacheResult.h"
#include "dawn/native/Device.h"
#include "dawn/native/Error.h"

namespace dawn::native {

namespace detail {

template <typename T>
struct UnwrapResultOrError {
    using type = T;
};

template <typename T>
struct UnwrapResultOrError<ResultOrError<T>> {
    using type = T;
};

template <typename T>
struct IsResultOrError {
    static constexpr bool value = false;
};

template <typename T>
struct IsResultOrError<ResultOrError<T>> {
    static constexpr bool value = true;
};

template <typename MaybeResultCacheHitRV, typename MaybeResultCreateRV>
struct MakeCacheResultImpl {
    using CacheHitRV = typename UnwrapResultOrError<MaybeResultCacheHitRV>::type;
    using CreateRV = typename UnwrapResultOrError<MaybeResultCreateRV>::type;

    static_assert(std::is_same_v<CacheHitRV, CreateRV>);

    using UnwrappedType = CacheResult<CreateRV>;

    using Type = std::conditional_t<IsResultOrError<MaybeResultCreateRV>::value,
                                    ResultOrError<UnwrappedType>,
                                    UnwrappedType>;

    Type operator()(CacheKey key, CacheHitRV value, CacheHitTag tag) {
        return UnwrappedType(std::move(key), std::move(value), tag);
    }

    Type operator()(CacheKey key, CreateRV value, CacheMissTag tag) {
        return UnwrappedType(std::move(key), std::move(value), tag);
    }

    Type operator()(CacheKey key, ResultOrError<CreateRV> valueOrError, CacheMissTag tag) {
        if (DAWN_LIKELY(valueOrError.IsSuccess())) {
            return UnwrappedType(std::move(key), valueOrError.AcquireSuccess(), tag);
        }
        return valueOrError.AcquireError();
    }
};

void LogCacheHitError(std::unique_ptr<ErrorData> error);

}  // namespace detail

// Implementation of a CacheRequest which provides a LoadOrCreate friend function which can be found
// via argument-dependent lookup. So, it doesn't need to be called with a fully qualified function
// name.
//
// Example usage:
//   Request r = { ... };
//   CacheResult<T> cacheResult =
//       LoadOrCreate(device, std::move(r),
//                    [](Blob blob) { /* handle cache hit */ },
//                    [](Request r) { /* handle cache miss */ }
//       );
//
// LoadOrCreate generates a CacheKey from the request and loads from the BlobCache. On cache hit,
// calls CacheHitFn and returns a CacheResult<T>. On cache miss or if CacheHitFn returned an Error,
// calls CreateFn -> T with the request data and returns a CacheResult<T>.
// If CreateFn returns a ResultOrError<T>, then T is ResultOrError<CacheResult<T>>, not
// CacheResult<ResultOrError<T>>. CacheHitFn must return T or ResultOrError<T>.
template <typename Request>
class CacheRequestImpl {
  public:
    CacheRequestImpl() = default;

    // Require CacheRequests to be move-only to avoid unnecessary copies.
    CacheRequestImpl(CacheRequestImpl&&) = default;
    CacheRequestImpl& operator=(CacheRequestImpl&&) = default;
    CacheRequestImpl(const CacheRequestImpl&) = delete;
    CacheRequestImpl& operator=(const CacheRequestImpl&) = delete;

    template <typename CacheHitFn, typename CreateFn>
    friend auto LoadOrCreate(DeviceBase* device,
                             Request r,
                             CacheHitFn cacheHitFn,
                             CreateFn createFn) {
        CacheKey key = r.CreateCacheKey(device);
        Blob blob = device->LoadCachedBlob(key);

        // Get return types and check that CreateRV can be cast to a raw function pointer.
        // This means it's not a std::function or lambda that captures additional data.
        using CacheHitRV = decltype(cacheHitFn(std::declval<Blob>()));
        using CreateRV = decltype(createFn(std::declval<Request>()));
        static_assert(std::is_convertible_v<CreateFn, CreateRV (*)(Request)>,
                      "CreateFn function signature does not match, or it is not a free function.");
        auto MakeCacheResult = detail::MakeCacheResultImpl<CacheHitRV, CreateRV>{};
        if (!blob.Empty()) {
            // Cache hit. Handle the cached blob.
            auto rv = cacheHitFn(std::move(blob));

            if constexpr (!detail::IsResultOrError<CacheHitRV>::value) {
                // If the cache hit return value is not a ResultOrError, return it directly.
                return MakeCacheResult(std::move(key), std::move(rv), CacheHitTag{});
            } else {
                // Otherwise, if the value is a success, also return it directly.
                if (DAWN_LIKELY(rv.IsSuccess())) {
                    return MakeCacheResult(std::move(key), rv.AcquireSuccess(), CacheHitTag{});
                }
                // Otherwise, continue to the cache miss path and log the error.
                detail::LogCacheHitError(rv.AcquireError());
            }
        }
        // Cache miss, or deserializing the cached blob failed.
        return MakeCacheResult(std::move(key), createFn(std::move(r)), CacheMissTag{});
    }
};

// Helper for X macro to declare a struct member.
#define DAWN_INTERNAL_CACHE_REQUEST_DECL_STRUCT_MEMBER(type, name) type name;

// Helper for X macro for recording cache request fields into a CacheKey.
#define DAWN_INTERNAL_CACHE_REQUEST_RECORD_KEY(type, name) key.Record(name);

// Helper macro to define a CacheRequest struct.
#define DAWN_MAKE_CACHE_REQUEST(Request, MEMBERS)                     \
    class Request : public CacheRequestImpl<Request> {                \
      public:                                                         \
        Request() = default;                                          \
        MEMBERS(DAWN_INTERNAL_CACHE_REQUEST_DECL_STRUCT_MEMBER)       \
      private:                                                        \
        friend class CacheRequestImpl<Request>;                       \
        /* Create a CacheKey from the request type and all members */ \
        CacheKey CreateCacheKey(const DeviceBase* device) const {     \
            CacheKey key = device->GetCacheKey();                     \
            key.Record(CacheKey::Type::Request);                      \
            MEMBERS(DAWN_INTERNAL_CACHE_REQUEST_RECORD_KEY)           \
            return key;                                               \
        }                                                             \
    };

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_CACHEREQUEST_H_
