// Copyright 2022 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0(the "License");
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

#ifndef SRC_TINT_CONSTANT_SCALAR_H_
#define SRC_TINT_CONSTANT_SCALAR_H_

#include "src/tint/constant/constant.h"
#include "src/tint/number.h"
#include "src/tint/utils/hash.h"

namespace tint::constant {

/// Scalar holds a single scalar or abstract-numeric value.
template <typename T>
class Scalar : public Constant {
  public:
    static_assert(!std::is_same_v<UnwrapNumber<T>, T> || std::is_same_v<T, bool>,
                  "T must be a Number or bool");

    Scalar(const type::Type* t, T v);
    ~Scalar() override;

    const type::Type* Type() const override { return type; }

    std::variant<std::monostate, AInt, AFloat> Value() const override {
        if constexpr (IsFloatingPoint<UnwrapNumber<T>>) {
            return static_cast<AFloat>(value);
        } else {
            return static_cast<AInt>(value);
        }
    }

    const Constant* Index(size_t) const override { return nullptr; }

    bool AllZero() const override { return IsPositiveZero(value); }
    bool AnyZero() const override { return IsPositiveZero(value); }
    bool AllEqual() const override { return true; }

    size_t Hash() const override { return utils::Hash(type, ValueOf(value)); }

    utils::Result<const Constant*> Convert(ProgramBuilder& builder,
                                                       const type::Type* target_ty,
                                                       const Source& source) const override;

    type::Type const* const type;
    const T value;
};

}  // namespace tint::constant

#endif  // SRC_TINT_CONSTANT_SCALAR_H_
