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

#include "src/tint/type/tuple.h"

#include <utility>

#include "src/tint/type/manager.h"
#include "src/tint/utils/string.h"
#include "src/tint/utils/transform.h"

TINT_INSTANTIATE_TYPEINFO(tint::type::Tuple);

namespace tint::type {

Tuple::Tuple(utils::VectorRef<const Type*> types)
    : Base(static_cast<size_t>(utils::Hash(utils::TypeInfo::Of<Tuple>().full_hashcode, types)),
           type::Flags{}),
      types_(std::move(types)) {}

Tuple::~Tuple() = default;

bool Tuple::Equals(const UniqueNode& other) const {
    if (auto* t = other.As<Tuple>()) {
        return types_ == t->types_;
    }
    return false;
}

std::string Tuple::FriendlyName() const {
    auto names = utils::Transform(types_, [](const Type* t) { return t->FriendlyName(); });
    return "[" + utils::Join(names, ", ") + "]";
}

Tuple* Tuple::Clone(CloneContext& ctx) const {
    auto tys = utils::Transform(types_, [&](const Type* t) { return t->Clone(ctx); });
    return ctx.dst.mgr->Get<Tuple>(std::move(tys));
}

}  // namespace tint::type
