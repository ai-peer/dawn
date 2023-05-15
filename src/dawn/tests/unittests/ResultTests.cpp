// Copyright 2018 The Dawn Authors
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

#include "dawn/common/RefCounted.h"
#include "dawn/common/Result.h"
#include "gtest/gtest.h"

namespace {

template <typename T, typename E>
void TestError(dawn::Result<T, E>* result, E expectedError) {
    EXPECT_TRUE(result->IsError());
    EXPECT_FALSE(result->IsSuccess());

    std::unique_ptr<E> storedError = result->AcquireError();
    EXPECT_EQ(*storedError, expectedError);
}

template <typename T, typename E>
void TestSuccess(dawn::Result<T, E>* result, T expectedSuccess) {
    EXPECT_FALSE(result->IsError());
    EXPECT_TRUE(result->IsSuccess());

    const T storedSuccess = result->AcquireSuccess();
    EXPECT_EQ(storedSuccess, expectedSuccess);

    // Once the success is acquired, result has an empty
    // payload and is neither in the success nor error state.
    EXPECT_FALSE(result->IsError());
    EXPECT_FALSE(result->IsSuccess());
}

static int placeholderError = 0xbeef;
static float placeholderSuccess = 42.0f;
static const float placeholderConstSuccess = 42.0f;

class AClass : public dawn::RefCounted {
  public:
    int a = 0;
};

// Tests using the following overload of TestSuccess make
// local Ref instances to placeholderSuccessObj. Tests should
// ensure any local Ref objects made along the way continue
// to point to placeholderSuccessObj.
template <typename T, typename E>
void TestSuccess(dawn::Result<dawn::Ref<T>, E>* result, T* expectedSuccess) {
    EXPECT_FALSE(result->IsError());
    EXPECT_TRUE(result->IsSuccess());

    // AClass starts with a reference count of 1 and stored
    // on the stack in the caller. The result parameter should
    // hold the only other reference to the object.
    EXPECT_EQ(expectedSuccess->GetRefCountForTesting(), 2u);

    const dawn::Ref<T> storedSuccess = result->AcquireSuccess();
    EXPECT_EQ(storedSuccess.Get(), expectedSuccess);

    // Once the success is acquired, result has an empty
    // payload and is neither in the success nor error state.
    EXPECT_FALSE(result->IsError());
    EXPECT_FALSE(result->IsSuccess());

    // Once we call AcquireSuccess, result no longer stores
    // the object. storedSuccess should contain the only other
    // reference to the object.
    EXPECT_EQ(storedSuccess->GetRefCountForTesting(), 2u);
}

// dawn::Result<void, E*>

// Test constructing an error dawn::Result<void, E>
TEST(ResultOnlyPointerError, ConstructingError) {
    dawn::Result<void, int> result(std::make_unique<int>(placeholderError));
    TestError(&result, placeholderError);
}

// Test moving an error dawn::Result<void, E>
TEST(ResultOnlyPointerError, MovingError) {
    dawn::Result<void, int> result(std::make_unique<int>(placeholderError));
    dawn::Result<void, int> movedResult(std::move(result));
    TestError(&movedResult, placeholderError);
}

// Test returning an error dawn::Result<void, E>
TEST(ResultOnlyPointerError, ReturningError) {
    auto CreateError = []() -> dawn::Result<void, int> {
        return {std::make_unique<int>(placeholderError)};
    };

    dawn::Result<void, int> result = CreateError();
    TestError(&result, placeholderError);
}

// Test constructing a success dawn::Result<void, E>
TEST(ResultOnlyPointerError, ConstructingSuccess) {
    dawn::Result<void, int> result;
    EXPECT_TRUE(result.IsSuccess());
    EXPECT_FALSE(result.IsError());
}

// Test moving a success dawn::Result<void, E>
TEST(ResultOnlyPointerError, MovingSuccess) {
    dawn::Result<void, int> result;
    dawn::Result<void, int> movedResult(std::move(result));
    EXPECT_TRUE(movedResult.IsSuccess());
    EXPECT_FALSE(movedResult.IsError());
}

// Test returning a success dawn::Result<void, E>
TEST(ResultOnlyPointerError, ReturningSuccess) {
    auto CreateError = []() -> dawn::Result<void, int> { return {}; };

    dawn::Result<void, int> result = CreateError();
    EXPECT_TRUE(result.IsSuccess());
    EXPECT_FALSE(result.IsError());
}

// dawn::Result<T*, E*>

// Test constructing an error dawn::Result<T*, E>
TEST(ResultBothPointer, ConstructingError) {
    dawn::Result<float*, int> result(std::make_unique<int>(placeholderError));
    TestError(&result, placeholderError);
}

// Test moving an error dawn::Result<T*, E>
TEST(ResultBothPointer, MovingError) {
    dawn::Result<float*, int> result(std::make_unique<int>(placeholderError));
    dawn::Result<float*, int> movedResult(std::move(result));
    TestError(&movedResult, placeholderError);
}

// Test returning an error dawn::Result<T*, E>
TEST(ResultBothPointer, ReturningError) {
    auto CreateError = []() -> dawn::Result<float*, int> {
        return {std::make_unique<int>(placeholderError)};
    };

    dawn::Result<float*, int> result = CreateError();
    TestError(&result, placeholderError);
}

// Test constructing a success dawn::Result<T*, E>
TEST(ResultBothPointer, ConstructingSuccess) {
    dawn::Result<float*, int> result(&placeholderSuccess);
    TestSuccess(&result, &placeholderSuccess);
}

// Test moving a success dawn::Result<T*, E>
TEST(ResultBothPointer, MovingSuccess) {
    dawn::Result<float*, int> result(&placeholderSuccess);
    dawn::Result<float*, int> movedResult(std::move(result));
    TestSuccess(&movedResult, &placeholderSuccess);
}

// Test returning a success dawn::Result<T*, E>
TEST(ResultBothPointer, ReturningSuccess) {
    auto CreateSuccess = []() -> dawn::Result<float*, int*> { return {&placeholderSuccess}; };

    dawn::Result<float*, int*> result = CreateSuccess();
    TestSuccess(&result, &placeholderSuccess);
}

// Tests converting from a dawn::Result<TChild*, E>
TEST(ResultBothPointer, ConversionFromChildClass) {
    struct T {
        int a;
    };
    struct TChild : T {};

    TChild child;
    T* childAsT = &child;
    {
        dawn::Result<T*, int> result(&child);
        TestSuccess(&result, childAsT);
    }
    {
        dawn::Result<TChild*, int> resultChild(&child);
        dawn::Result<T*, int> result(std::move(resultChild));
        TestSuccess(&result, childAsT);
    }
    {
        dawn::Result<TChild*, int> resultChild(&child);
        dawn::Result<T*, int> result = std::move(resultChild);
        TestSuccess(&result, childAsT);
    }
}

// dawn::Result<const T*, E>

// Test constructing an error dawn::Result<const T*, E>
TEST(ResultBothPointerWithConstResult, ConstructingError) {
    dawn::Result<const float*, int> result(std::make_unique<int>(placeholderError));
    TestError(&result, placeholderError);
}

// Test moving an error dawn::Result<const T*, E>
TEST(ResultBothPointerWithConstResult, MovingError) {
    dawn::Result<const float*, int> result(std::make_unique<int>(placeholderError));
    dawn::Result<const float*, int> movedResult(std::move(result));
    TestError(&movedResult, placeholderError);
}

// Test returning an error dawn::Result<const T*, E*>
TEST(ResultBothPointerWithConstResult, ReturningError) {
    auto CreateError = []() -> dawn::Result<const float*, int> {
        return {std::make_unique<int>(placeholderError)};
    };

    dawn::Result<const float*, int> result = CreateError();
    TestError(&result, placeholderError);
}

// Test constructing a success dawn::Result<const T*, E*>
TEST(ResultBothPointerWithConstResult, ConstructingSuccess) {
    dawn::Result<const float*, int> result(&placeholderConstSuccess);
    TestSuccess(&result, &placeholderConstSuccess);
}

// Test moving a success dawn::Result<const T*, E*>
TEST(ResultBothPointerWithConstResult, MovingSuccess) {
    dawn::Result<const float*, int> result(&placeholderConstSuccess);
    dawn::Result<const float*, int> movedResult(std::move(result));
    TestSuccess(&movedResult, &placeholderConstSuccess);
}

// Test returning a success dawn::Result<const T*, E*>
TEST(ResultBothPointerWithConstResult, ReturningSuccess) {
    auto CreateSuccess = []() -> dawn::Result<const float*, int> {
        return {&placeholderConstSuccess};
    };

    dawn::Result<const float*, int> result = CreateSuccess();
    TestSuccess(&result, &placeholderConstSuccess);
}

// dawn::Result<dawn::Ref<T>, E>

// Test constructing an error dawn::Result<dawn::Ref<T>, E>
TEST(ResultRefT, ConstructingError) {
    dawn::Result<dawn::Ref<AClass>, int> result(std::make_unique<int>(placeholderError));
    TestError(&result, placeholderError);
}

// Test moving an error dawn::Result<dawn::Ref<T>, E>
TEST(ResultRefT, MovingError) {
    dawn::Result<dawn::Ref<AClass>, int> result(std::make_unique<int>(placeholderError));
    dawn::Result<dawn::Ref<AClass>, int> movedResult(std::move(result));
    TestError(&movedResult, placeholderError);
}

// Test returning an error dawn::Result<dawn::Ref<T>, E>
TEST(ResultRefT, ReturningError) {
    auto CreateError = []() -> dawn::Result<dawn::Ref<AClass>, int> {
        return {std::make_unique<int>(placeholderError)};
    };

    dawn::Result<dawn::Ref<AClass>, int> result = CreateError();
    TestError(&result, placeholderError);
}

// Test constructing a success dawn::Result<dawn::Ref<T>, E>
TEST(ResultRefT, ConstructingSuccess) {
    AClass success;

    dawn::Ref<AClass> refObj(&success);
    dawn::Result<dawn::Ref<AClass>, int> result(std::move(refObj));
    TestSuccess(&result, &success);
}

// Test moving a success dawn::Result<dawn::Ref<T>, E>
TEST(ResultRefT, MovingSuccess) {
    AClass success;

    dawn::Ref<AClass> refObj(&success);
    dawn::Result<dawn::Ref<AClass>, int> result(std::move(refObj));
    dawn::Result<dawn::Ref<AClass>, int> movedResult(std::move(result));
    TestSuccess(&movedResult, &success);
}

// Test returning a success dawn::Result<dawn::Ref<T>, E>
TEST(ResultRefT, ReturningSuccess) {
    AClass success;
    auto CreateSuccess = [&success]() -> dawn::Result<dawn::Ref<AClass>, int> {
        return dawn::Ref<AClass>(&success);
    };

    dawn::Result<dawn::Ref<AClass>, int> result = CreateSuccess();
    TestSuccess(&result, &success);
}

class OtherClass {
  public:
    int a = 0;
};
class Base : public dawn::RefCounted {};
class Child : public OtherClass, public Base {};

// Test constructing a dawn::Result<dawn::Ref<TChild>, E>
TEST(ResultRefT, ConversionFromChildConstructor) {
    Child child;
    dawn::Ref<Child> refChild(&child);

    dawn::Result<dawn::Ref<Base>, int> result(std::move(refChild));
    TestSuccess<Base>(&result, &child);
}

// Test copy constructing dawn::Result<dawn::Ref<TChild>, E>
TEST(ResultRefT, ConversionFromChildCopyConstructor) {
    Child child;
    dawn::Ref<Child> refChild(&child);

    dawn::Result<dawn::Ref<Child>, int> resultChild(std::move(refChild));
    dawn::Result<dawn::Ref<Base>, int> result(std::move(resultChild));
    TestSuccess<Base>(&result, &child);
}

// Test assignment operator for dawn::Result<dawn::Ref<TChild>, E>
TEST(ResultRefT, ConversionFromChildAssignmentOperator) {
    Child child;
    dawn::Ref<Child> refChild(&child);

    dawn::Result<dawn::Ref<Child>, int> resultChild(std::move(refChild));
    dawn::Result<dawn::Ref<Base>, int> result = std::move(resultChild);
    TestSuccess<Base>(&result, &child);
}

// dawn::Result<T, E>

// Test constructing an error dawn::Result<T, E>
TEST(ResultGeneric, ConstructingError) {
    dawn::Result<std::vector<float>, int> result(std::make_unique<int>(placeholderError));
    TestError(&result, placeholderError);
}

// Test moving an error dawn::Result<T, E>
TEST(ResultGeneric, MovingError) {
    dawn::Result<std::vector<float>, int> result(std::make_unique<int>(placeholderError));
    dawn::Result<std::vector<float>, int> movedResult(std::move(result));
    TestError(&movedResult, placeholderError);
}

// Test returning an error dawn::Result<T, E>
TEST(ResultGeneric, ReturningError) {
    auto CreateError = []() -> dawn::Result<std::vector<float>, int> {
        return {std::make_unique<int>(placeholderError)};
    };

    dawn::Result<std::vector<float>, int> result = CreateError();
    TestError(&result, placeholderError);
}

// Test constructing a success dawn::Result<T, E>
TEST(ResultGeneric, ConstructingSuccess) {
    dawn::Result<std::vector<float>, int> result({1.0f});
    TestSuccess(&result, {1.0f});
}

// Test moving a success dawn::Result<T, E>
TEST(ResultGeneric, MovingSuccess) {
    dawn::Result<std::vector<float>, int> result({1.0f});
    dawn::Result<std::vector<float>, int> movedResult(std::move(result));
    TestSuccess(&movedResult, {1.0f});
}

// Test returning a success dawn::Result<T, E>
TEST(ResultGeneric, ReturningSuccess) {
    auto CreateSuccess = []() -> dawn::Result<std::vector<float>, int> { return {{1.0f}}; };

    dawn::Result<std::vector<float>, int> result = CreateSuccess();
    TestSuccess(&result, {1.0f});
}

}  // anonymous namespace
