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

#include <memory>
#include <utility>
#include <vector>

#include "dawn/native/CacheRequest.h"
#include "dawn/native/Instance.h"
#include "dawn/tests/DawnNativeTest.h"
#include "dawn/tests/mocks/platform/CachingInterfaceMock.h"

namespace dawn::native {

namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

class CacheRequestTests : public DawnNativeTest {
  protected:
    CacheRequestTests() : DawnNativeTest() {
        mPlatform = std::make_unique<DawnCachingMockPlatform>(&mMockCache);
        dawn::native::FromAPI(instance->Get())->SetPlatformForTesting(mPlatform.get());
    }

    WGPUDevice CreateTestDevice() override {
        wgpu::DeviceDescriptor deviceDescriptor = {};
        wgpu::DawnTogglesDeviceDescriptor togglesDesc = {};
        deviceDescriptor.nextInChain = &togglesDesc;

        const char* toggle = "enable_blob_cache";
        togglesDesc.forceEnabledToggles = &toggle;
        togglesDesc.forceEnabledTogglesCount = 1;

        return adapter.CreateDevice(&deviceDescriptor);
    }

    DeviceBase* GetDevice() { return dawn::native::FromAPI(device.Get()); }

    template <typename Expected, typename V>
    void CheckType(V v) {
        static_assert(std::is_same_v<Expected, V>);
    }

    template <typename Expected, typename V>
    void CheckType(ResultOrError<V> v) {
        static_assert(std::is_same_v<Expected, ResultOrError<V>>);
        // ResultOrError must be acquired.
        v.AcquireSuccess();
    }

    StrictMock<CachingInterfaceMock> mMockCache;

  private:
    std::unique_ptr<dawn::platform::Platform> mPlatform;
};

struct Foo {
    int value;
};

#define REQUEST_MEMBERS(X)                   \
    X(int, a)                                \
    X(float, b)                              \
    X(std::vector<unsigned int>, c)          \
    X(CacheKey::UnsafeUnkeyedValue<int*>, d) \
    X(CacheKey::UnsafeUnkeyedValue<Foo>, e)

DAWN_MAKE_CACHE_REQUEST(CacheRequestForTesting, REQUEST_MEMBERS)

#undef REQUEST_MEMBERS

// static_assert the expected types for various return types from
// the cache hit handler and cache miss handler.
TEST_F(CacheRequestTests, CacheResultTypes) {
#define MAKE_CASE(CacheHitType, CacheMissType)                                         \
    LoadOrCreate(                                                                      \
        GetDevice(), CacheRequestForTesting{}, [](Blob) -> CacheHitType { return 0; }, \
        [](CacheRequestForTesting) -> CacheMissType { return 1; })

    EXPECT_CALL(mMockCache, LoadData(_, _, nullptr, 0)).WillRepeatedly(Return(0));

    // (int, int), should be int.
    CheckType<CacheResult<int>>(MAKE_CASE(int, int));

    // (ResultOrError<int>, int), should be int.
    // Error on the cache hit type doesn't also make it ResultOrError.
    CheckType<CacheResult<int>>(MAKE_CASE(ResultOrError<int>, int));

    // (int, ResultOrError<int>), should be ResultOrError<int>.
    // Error on the cache miss type makes it ResultOrError.
    CheckType<ResultOrError<CacheResult<int>>>(MAKE_CASE(int, ResultOrError<int>));

    // (ResultOrError<int>, ResultOrError<int>), should be ResultOrError<int>.
    // Error on the cache miss type makes it ResultOrError.
    CheckType<ResultOrError<CacheResult<int>>>(MAKE_CASE(ResultOrError<int>, ResultOrError<int>));
#undef MAKE_CASE
}

// Test that using a CacheRequest builds a key from the device key,
// the request type enum, and all of the request members.
TEST_F(CacheRequestTests, MakesCacheKey) {
    // Make a request.
    CacheRequestForTesting req;
    req.a = 1;
    req.b = 0.2;
    req.c = {3, 4, 5};

    // Make the expected key.
    CacheKey expectedKey;
    expectedKey.Record(GetDevice()->GetCacheKey(), CacheKey::Type::CacheRequestForTesting, req.a,
                       req.b, req.c);

    // Expect a call to LoadData with the expected key.
    EXPECT_CALL(mMockCache, LoadData(_, expectedKey.size(), nullptr, 0))
        .WillOnce(WithArg<0>(Invoke([&](const void* actualKeyData) {
            EXPECT_EQ(memcmp(actualKeyData, expectedKey.data(), expectedKey.size()), 0);
            return 0;
        })));

    // Load the request.
    auto result = LoadOrCreate(
        GetDevice(), std::move(req), [](Blob) -> int { return 0; },
        [](CacheRequestForTesting) -> int { return 0; });

    // The created cache key should be saved on the result.
    EXPECT_EQ(result.GetCacheKey().size(), expectedKey.size());
    EXPECT_EQ(memcmp(result.GetCacheKey().data(), expectedKey.data(), expectedKey.size()), 0);
}

// Test that members that are wrapped in UnsafeUnkeyedValue do not impact the key.
TEST_F(CacheRequestTests, CacheKeyIgnoresUnsafeIgnoredValue) {
    // Make two requests with different UnsafeUnkeyedValues.
    int v1, v2;
    CacheRequestForTesting req1;
    req1.d = UnsafeUnkeyedValue(&v1);
    req1.e = UnsafeUnkeyedValue(Foo{42});

    CacheRequestForTesting req2;
    req2.d = UnsafeUnkeyedValue(&v2);
    req2.e = UnsafeUnkeyedValue(Foo{24});

    EXPECT_CALL(mMockCache, LoadData(_, _, nullptr, 0)).WillOnce(Return(0)).WillOnce(Return(0));

    // Load the requests
    auto r1 = LoadOrCreate(
        GetDevice(), std::move(req1), [](Blob) { return 0; },
        [](CacheRequestForTesting) { return 0; });
    auto r2 = LoadOrCreate(
        GetDevice(), std::move(req2), [](Blob) { return 0; },
        [](CacheRequestForTesting) { return 0; });

    // Expect their keys to be the same.
    EXPECT_EQ(r1.GetCacheKey().size(), r2.GetCacheKey().size());
    EXPECT_EQ(memcmp(r1.GetCacheKey().data(), r2.GetCacheKey().data(), r1.GetCacheKey().size()), 0);
}

// Test the expected code path when there is a cache miss.
TEST_F(CacheRequestTests, CacheMiss) {
    // Make a request.
    CacheRequestForTesting req;
    req.a = 1;
    req.b = 0.2;
    req.c = {3, 4, 5};

    unsigned int* cPtr = req.c.data();

    static StrictMock<MockFunction<int(Blob)>> cacheHitFn;
    static StrictMock<MockFunction<int(CacheRequestForTesting)>> cacheMissFn;

    // Mock a cache miss.
    EXPECT_CALL(mMockCache, LoadData(_, _, nullptr, 0)).WillOnce(Return(0));

    // Expect the cache miss, and return some value.
    int rv = 42;
    EXPECT_CALL(cacheMissFn, Call(_)).WillOnce(WithArg<0>(Invoke([=](CacheRequestForTesting req) {
        // Expect the request contents to be the same. The data pointer
        // for |c| is also the same since it was moved.
        EXPECT_EQ(req.a, 1);
        EXPECT_FLOAT_EQ(req.b, 0.2);
        EXPECT_EQ(req.c.data(), cPtr);
        return rv;
    })));

    // Load the request.
    auto result = LoadOrCreate(
        GetDevice(), std::move(req),
        [](Blob blob) -> int { return cacheHitFn.Call(std::move(blob)); },
        [](CacheRequestForTesting req) -> int { return cacheMissFn.Call(std::move(req)); });

    // Expect the result to store the value.
    EXPECT_EQ(*result, rv);
    EXPECT_FALSE(result.IsCached());
}

// Test the expected code path when there is a cache hit.
TEST_F(CacheRequestTests, CacheHit) {
    // Make a request.
    CacheRequestForTesting req;
    req.a = 1;
    req.b = 0.2;
    req.c = {3, 4, 5};

    static StrictMock<MockFunction<int(Blob)>> cacheHitFn;
    static StrictMock<MockFunction<int(CacheRequestForTesting)>> cacheMissFn;

    static constexpr char kCachedData[] = "hello world!";

    // Mock a cache hit, and load the cached data.
    EXPECT_CALL(mMockCache, LoadData(_, _, nullptr, 0)).WillOnce(Return(sizeof(kCachedData)));
    EXPECT_CALL(mMockCache, LoadData(_, _, _, sizeof(kCachedData)))
        .WillOnce(WithArg<2>(Invoke([](void* dataOut) {
            memcpy(dataOut, kCachedData, sizeof(kCachedData));
            return sizeof(kCachedData);
        })));

    // Expect the cache hit, and return some value.
    int rv = 1337;
    EXPECT_CALL(cacheHitFn, Call(_)).WillOnce(WithArg<0>(Invoke([=](Blob blob) {
        // Expect the cached blob contents to match the cached data.
        EXPECT_EQ(blob.Size(), sizeof(kCachedData));
        EXPECT_EQ(memcmp(blob.Data(), kCachedData, sizeof(kCachedData)), 0);

        return rv;
    })));

    // Load the request.
    auto result = LoadOrCreate(
        GetDevice(), std::move(req),
        [](Blob blob) -> int { return cacheHitFn.Call(std::move(blob)); },
        [](CacheRequestForTesting req) -> int { return cacheMissFn.Call(std::move(req)); });

    // Expect the result to store the value.
    EXPECT_EQ(*result, rv);
    EXPECT_TRUE(result.IsCached());
}

// Test the expected code path when there is a cache hit but the handler errors.
TEST_F(CacheRequestTests, CacheHitError) {
    // Make a request.
    CacheRequestForTesting req;
    req.a = 1;
    req.b = 0.2;
    req.c = {3, 4, 5};

    unsigned int* cPtr = req.c.data();

    static StrictMock<MockFunction<ResultOrError<int>(Blob)>> cacheHitFn;
    static StrictMock<MockFunction<int(CacheRequestForTesting)>> cacheMissFn;

    static constexpr char kCachedData[] = "hello world!";

    // Mock a cache hit, and load the cached data.
    EXPECT_CALL(mMockCache, LoadData(_, _, nullptr, 0)).WillOnce(Return(sizeof(kCachedData)));
    EXPECT_CALL(mMockCache, LoadData(_, _, _, sizeof(kCachedData)))
        .WillOnce(WithArg<2>(Invoke([](void* dataOut) {
            memcpy(dataOut, kCachedData, sizeof(kCachedData));
            return sizeof(kCachedData);
        })));

    // Expect the cache hit.
    EXPECT_CALL(cacheHitFn, Call(_)).WillOnce(WithArg<0>(Invoke([=](Blob blob) {
        // Expect the cached blob contents to match the cached data.
        EXPECT_EQ(blob.Size(), sizeof(kCachedData));
        EXPECT_EQ(memcmp(blob.Data(), kCachedData, sizeof(kCachedData)), 0);

        // Return an error.
        return DAWN_VALIDATION_ERROR("fake test error");
    })));

    // Expect the cache miss handler since the cache hit errored.
    int rv = 79;
    EXPECT_CALL(cacheMissFn, Call(_)).WillOnce(WithArg<0>(Invoke([=](CacheRequestForTesting req) {
        // Expect the request contents to be the same. The data pointer
        // for |c| is also the same since it was moved.
        EXPECT_EQ(req.a, 1);
        EXPECT_FLOAT_EQ(req.b, 0.2);
        EXPECT_EQ(req.c.data(), cPtr);
        return rv;
    })));

    // Load the request.
    auto result = LoadOrCreate(
        GetDevice(), std::move(req),
        [](Blob blob) -> ResultOrError<int> { return cacheHitFn.Call(std::move(blob)); },
        [](CacheRequestForTesting req) -> int { return cacheMissFn.Call(std::move(req)); });

    // Expect the result to store the value.
    EXPECT_EQ(*result, rv);
    EXPECT_FALSE(result.IsCached());
}

}  // namespace

}  // namespace dawn::native
