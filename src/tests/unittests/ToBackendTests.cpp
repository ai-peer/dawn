// Copyright 2017 The Dawn Authors
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

#include <gtest/gtest.h>

#include "dawn_native/RefCounted.h"
#include "dawn_native/ToBackend.h"

#include <type_traits>

// Make our own Base - Backend object pair, reusing the BindGroup name
namespace dawn_native {
    class BindGroupBase : public RefCounted {
    };
}

using namespace dawn_native;

class MyBindGroup : public BindGroupBase {
};

struct MyBackendTraits {
    using BindGroupType = MyBindGroup;
};

// Instanciate ToBackend for our "backend"
template<typename T>
auto ToBackend(T&& common) -> decltype(ToBackendBase<MyBackendTraits>(common)) {
    return ToBackendBase<MyBackendTraits>(common);
}

// Test that ToBackend correctly converts pointers to base classes.
TEST(ToBackend, Pointers) {
    {
        MyBindGroup* bindGroup = new MyBindGroup;
        const BindGroupBase* base = bindGroup;

        auto backendBindGroup = ToBackend(base);
        static_assert(std::is_same<decltype(backendBindGroup), const MyBindGroup*>::value, "");
        ASSERT_EQ(bindGroup, backendBindGroup);

        bindGroup->Release();
    }
    {
        MyBindGroup* bindGroup = new MyBindGroup;
        BindGroupBase* base = bindGroup;

        auto backendBindGroup = ToBackend(base);
        static_assert(std::is_same<decltype(backendBindGroup), MyBindGroup*>::value, "");
        ASSERT_EQ(bindGroup, backendBindGroup);

        bindGroup->Release();
    }
}

// Test that ToBackend correctly converts Refs to base classes.
TEST(ToBackend, Ref) {
    {
        MyBindGroup* bindGroup = new MyBindGroup;
        const Ref<BindGroupBase> base(bindGroup);

        const auto& backendBindGroup = ToBackend(base);
        static_assert(std::is_same<decltype(ToBackend(base)), const Ref<MyBindGroup>&>::value, "");
        ASSERT_EQ(bindGroup, backendBindGroup.Get());

        bindGroup->Release();
    }
    {
        MyBindGroup* bindGroup = new MyBindGroup;
        Ref<BindGroupBase> base(bindGroup);

        auto backendBindGroup = ToBackend(base);
        static_assert(std::is_same<decltype(ToBackend(base)), Ref<MyBindGroup>&>::value, "");
        ASSERT_EQ(bindGroup, backendBindGroup.Get());

        bindGroup->Release();
    }
}
