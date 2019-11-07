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

// Make our own Base - Backend object pair, reusing the BindGroupLayout name
namespace dawn_native {
    class BindGroupLayoutBase : public RefCounted {
    };
}

using namespace dawn_native;

class MyBindGroupLayout : public BindGroupLayoutBase {
};

struct MyBackendTraits {
    using BindGroupLayoutType = MyBindGroupLayout;
};

// Instanciate ToBackend for our "backend"
template<typename T>
auto ToBackend(T&& common) -> decltype(ToBackendBase<MyBackendTraits>(common)) {
    return ToBackendBase<MyBackendTraits>(common);
}

// Test that ToBackend correctly converts pointers to base classes.
TEST(ToBackend, Pointers) {
    {
        MyBindGroupLayout* bindGroup = new MyBindGroupLayout;
        const BindGroupLayoutBase* base = bindGroup;

        auto backendBindGroupLayout = ToBackend(base);
        static_assert(std::is_same<decltype(backendBindGroupLayout), const MyBindGroupLayout*>::value, "");
        ASSERT_EQ(bindGroup, backendBindGroupLayout);

        bindGroup->Release();
    }
    {
        MyBindGroupLayout* bindGroup = new MyBindGroupLayout;
        BindGroupLayoutBase* base = bindGroup;

        auto backendBindGroupLayout = ToBackend(base);
        static_assert(std::is_same<decltype(backendBindGroupLayout), MyBindGroupLayout*>::value, "");
        ASSERT_EQ(bindGroup, backendBindGroupLayout);

        bindGroup->Release();
    }
}

// Test that ToBackend correctly converts Refs to base classes.
TEST(ToBackend, Ref) {
    {
        MyBindGroupLayout* bindGroup = new MyBindGroupLayout;
        const Ref<BindGroupLayoutBase> base(bindGroup);

        const auto& backendBindGroupLayout = ToBackend(base);
        static_assert(std::is_same<decltype(ToBackend(base)), const Ref<MyBindGroupLayout>&>::value, "");
        ASSERT_EQ(bindGroup, backendBindGroupLayout.Get());

        bindGroup->Release();
    }
    {
        MyBindGroupLayout* bindGroup = new MyBindGroupLayout;
        Ref<BindGroupLayoutBase> base(bindGroup);

        auto backendBindGroupLayout = ToBackend(base);
        static_assert(std::is_same<decltype(ToBackend(base)), Ref<MyBindGroupLayout>&>::value, "");
        ASSERT_EQ(bindGroup, backendBindGroupLayout.Get());

        bindGroup->Release();
    }
}
