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

#ifndef SRC_TINT_CONSTANT_COMPOSITE_H_
#define SRC_TINT_CONSTANT_COMPOSITE_H_

#include "src/tint/constant/constant.h"
#include "src/tint/number.h"
#include "src/tint/utils/hash.h"

namespace tint::constant {

/// Composite holds a number of mixed child Constant values.
/// Composite may be of a vector, matrix or array type.
///
/// If each element is the same type and value, then a Splat should be used.
class Composite : public Constant {
  public:
    Composite(const type::Type* t,
              utils::VectorRef<const constant::Constant*> els,
              bool all_0,
              bool any_0)
        : type(t), elements(std::move(els)), all_zero(all_0), any_zero(any_0), hash(CalcHash()) {}

    ~Composite() override;

    const type::Type* Type() const override { return type; }

    std::variant<std::monostate, AInt, AFloat> Value() const override { return {}; }

    const constant::Constant* Index(size_t i) const override {
        return i < elements.Length() ? elements[i] : nullptr;
    }

    bool AllZero() const override { return all_zero; }
    bool AnyZero() const override { return any_zero; }
    bool AllEqual() const override { return false; }

    size_t Hash() const override { return hash; }

    utils::Result<const Constant*> Convert(ProgramBuilder& builder,
                                                       const type::Type* target_ty,
                                                       const Source& source) const override;

    type::Type const* const type;
    utils::Vector<const constant::Constant*, 8> elements;
    const bool all_zero;
    const bool any_zero;
    const size_t hash;

  private:
    size_t CalcHash() {
        auto h = utils::Hash(type, all_zero, any_zero);
        for (auto* el : elements) {
            h = utils::HashCombine(h, el->Hash());
        }
        return h;
    }

};

}  // namespace tint::constant

#endif  // SRC_TINT_CONSTANT_COMPOSITE_H_


