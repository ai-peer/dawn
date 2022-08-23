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

#include "dawn/native/Destroyable.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace dawn::native {
namespace {

class ChildA : public Destroyable<ChildA, RefCounted> {
  public:
    explicit ChildA(Owns<ChildA>* owner) : Destroyable<ChildA, RefCounted>(owner) {}
    MOCK_METHOD(void, DestroyImpl, (), (override));
};

class ChildB : public Destroyable<ChildB, RefCounted> {
  public:
    explicit ChildB(Owns<ChildB>* owner) : Destroyable<ChildB, RefCounted>(owner) {}
    MOCK_METHOD(void, DestroyImpl, (), (override));
};

class RefCountedOwner : public Owner<RefCounted>, public Owns<ChildA>, public Owns<ChildB> {
  public:
    RefCountedOwner() : Owner<RefCounted>(), Owns<ChildA>(this), Owns<ChildB>(this) {}
};

// Tests that when Owner<RefCounted>::Destroy is called, it's children are destroyed.
TEST(DestroyableTests, RefCountedOwnerSingleExplicit) {
    RefCountedOwner owner;
    ChildA aPtr(&owner);
    EXPECT_CALL(aPtr, DestroyImpl).Times(1);
    owner.Destroy();
}

// Tests that when Owner<RefCounted> is implicitly destroyed by dropping it's last ref, it's
// children are destroyed.
TEST(DestroyableTests, RefCountedOwnerSingleImplicit) {
    Ref<RefCountedOwner> owner = AcquireRef(new RefCountedOwner());
    Ref<ChildA> aPtr = AcquireRef(new ChildA(owner.Get()));
    EXPECT_CALL(*aPtr.Get(), DestroyImpl).Times(1);
    owner = nullptr;
}

}  // namespace
}  // namespace dawn::native
