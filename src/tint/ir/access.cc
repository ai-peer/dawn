// Copyright 2023 The Tint Authors.
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

#include "src/tint/ir/access.h"

#include <utility>

#include "src/tint/debug.h"
#include "src/tint/ir/constant.h"
#include "src/tint/switch.h"
#include "src/tint/type/array.h"
#include "src/tint/type/manager.h"
#include "src/tint/type/matrix.h"
#include "src/tint/type/pointer.h"
#include "src/tint/type/struct.h"
#include "src/tint/type/vector.h"

TINT_INSTANTIATE_TYPEINFO(tint::ir::Access);

namespace tint::ir {

//! @cond Doxygen_Suppress
Access::Access(const type::Type* ty, Value* object, utils::VectorRef<Value*> indices)
    : result_type_(ty) {
    TINT_ASSERT(IR, object);
    TINT_ASSERT(IR, result_type_);
    TINT_ASSERT(IR, !indices.IsEmpty());

    AddOperand(object);
    AddOperands(std::move(indices));
}

Access::~Access() = default;
//! @endcond

utils::Vector<const type::Type*, 4> Access::SourceObjectTypes(type::Manager& ty) const {
    utils::Vector<const type::Type*, 4> types;
    auto* source_type = Object()->Type();
    auto* base_ptr = source_type->As<type::Pointer>();
    for (auto* idx : Indices()) {
        types.Push(source_type);
        source_type = tint::Switch(
            source_type->UnwrapPtr(),  //
            [&](const type::Array* arr) { return arr->ElemType(); },
            [&](const type::Matrix* mat) { return mat->ColumnType(); },
            [&](const type::Struct* str) {
                auto i = idx->As<Constant>()->Value()->ValueAs<u32>();
                return str->Members()[i]->Type();
            });
        if (base_ptr) {
            source_type = ty.pointer(source_type, base_ptr->AddressSpace(), base_ptr->Access());
        }
    }
    return types;
}

}  // namespace tint::ir
