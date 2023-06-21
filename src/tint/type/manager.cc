// Copyright 2022 The Tint Authors.
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

#include "src/tint/type/manager.h"

#include <algorithm>

#include "src/tint/type/abstract_float.h"
#include "src/tint/type/abstract_int.h"
#include "src/tint/type/array.h"
#include "src/tint/type/bool.h"
#include "src/tint/type/f16.h"
#include "src/tint/type/f32.h"
#include "src/tint/type/i32.h"
#include "src/tint/type/matrix.h"
#include "src/tint/type/pointer.h"
#include "src/tint/type/type.h"
#include "src/tint/type/u32.h"
#include "src/tint/type/vector.h"
#include "src/tint/type/void.h"

namespace tint::type {

Manager::Manager() = default;

Manager::Manager(Manager&&) = default;

Manager& Manager::operator=(Manager&& rhs) = default;

Manager::~Manager() = default;

type::Void* Manager::void_() {
    return Get<type::Void>();
}

type::Bool* Manager::bool_() {
    return Get<type::Bool>();
}

type::I32* Manager::i32() {
    return Get<type::I32>();
}

type::U32* Manager::u32() {
    return Get<type::U32>();
}

type::F32* Manager::f32() {
    return Get<type::F32>();
}

type::F16* Manager::f16() {
    return Get<type::F16>();
}

type::AbstractFloat* Manager::AFloat() {
    return Get<type::AbstractFloat>();
}

type::AbstractInt* Manager::AInt() {
    return Get<type::AbstractInt>();
}

type::Vector* Manager::vec(type::Type* inner, uint32_t size) {
    return Get<type::Vector>(inner, size);
}

type::Vector* Manager::vec2(type::Type* inner) {
    return vec(inner, 2);
}

type::Vector* Manager::vec3(type::Type* inner) {
    return vec(inner, 3);
}

type::Vector* Manager::vec4(type::Type* inner) {
    return vec(inner, 4);
}

type::Matrix* Manager::mat(type::Type* inner, uint32_t cols, uint32_t rows) {
    return Get<type::Matrix>(vec(inner, rows), cols);
}

type::Matrix* Manager::mat2x2(type::Type* inner) {
    return mat(inner, 2, 2);
}

type::Matrix* Manager::mat2x3(type::Type* inner) {
    return mat(inner, 2, 3);
}

type::Matrix* Manager::mat2x4(type::Type* inner) {
    return mat(inner, 2, 4);
}

type::Matrix* Manager::mat3x2(type::Type* inner) {
    return mat(inner, 3, 2);
}

type::Matrix* Manager::mat3x3(type::Type* inner) {
    return mat(inner, 3, 3);
}

type::Matrix* Manager::mat3x4(type::Type* inner) {
    return mat(inner, 3, 4);
}

type::Matrix* Manager::mat4x2(type::Type* inner) {
    return mat(inner, 4, 2);
}

type::Matrix* Manager::mat4x3(type::Type* inner) {
    return mat(inner, 4, 3);
}

type::Matrix* Manager::mat4x4(type::Type* inner) {
    return mat(inner, 4, 4);
}

type::Array* Manager::array(type::Type* elem_ty, uint32_t count, uint32_t stride /* = 0*/) {
    uint32_t implicit_stride = utils::RoundUp(elem_ty->Align(), elem_ty->Size());
    if (stride == 0) {
        stride = implicit_stride;
    }
    TINT_ASSERT(Type, stride >= implicit_stride);

    return Get<type::Array>(/* element type */ elem_ty,
                            /* element count */ Get<ConstantArrayCount>(count),
                            /* array alignment */ elem_ty->Align(),
                            /* array size */ count * stride,
                            /* element stride */ stride,
                            /* implicit stride */ implicit_stride);
}

type::Array* Manager::runtime_array(type::Type* elem_ty, uint32_t stride /* = 0 */) {
    if (stride == 0) {
        stride = elem_ty->Align();
    }
    return Get<type::Array>(
        /* element type */ elem_ty,
        /* element count */ Get<RuntimeArrayCount>(),
        /* array alignment */ elem_ty->Align(),
        /* array size */ stride,
        /* element stride */ stride,
        /* implicit stride */ elem_ty->Align());
}

type::Pointer* Manager::ptr(builtin::AddressSpace address_space,
                            type::Type* subtype,
                            builtin::Access access /* = builtin::Access::kReadWrite */) {
    return Get<type::Pointer>(address_space, subtype, access);
}

type::Struct* Manager::Struct(Symbol name, utils::VectorRef<StructMemberDesc> md) {
    utils::Vector<type::StructMember*, 4> members;
    uint32_t current_size = 0u;
    uint32_t max_align = 0u;
    for (const auto& m : md) {
        uint32_t index = static_cast<uint32_t>(members.Length());
        uint32_t offset = utils::RoundUp(m.type->Align(), current_size);
        members.Push(Get<type::StructMember>(m.name, m.type, index, offset, m.type->Align(),
                                             m.type->Size(), std::move(m.attributes)));
        current_size = offset + m.type->Size();
        max_align = std::max(max_align, m.type->Align());
    }
    return Get<type::Struct>(name, members, max_align, utils::RoundUp(max_align, current_size),
                             current_size);
}

}  // namespace tint::type
