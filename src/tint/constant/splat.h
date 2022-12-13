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

#ifndef SRC_TINT_CONSTANT_SPLAT_H_
#define SRC_TINT_CONSTANT_SPLAT_H_

#include "src/tint/constant/composite.h"
#include "src/tint/number.h"
#include "src/tint/utils/hash.h"

namespace tint::constant {

/// Splat holds a single Constant value, duplicated as all children.
/// Splat is used for zero-initializers, 'splat' initializers, or initializers where each element is
/// identical. Splat may be of a vector, matrix or array type.
class Splat : public Composite {
  public:
    Splat(const type::Type* t, const Constant* e, size_t n)
        : Composite(t, utils::Empty, e->AllZero(), e->AnyZero()), count(n) {
        elements.Push(e);
    }

    ~Splat() override;

    const constant::Constant* Index(size_t i) const override {
        return i < count ? elements[0] : nullptr;
    }

    bool AllEqual() const override { return true; }

    size_t Hash() const override { return utils::Hash(type, elements[0]->Hash(), count); }

    utils::Result<const Constant*> Convert(ProgramBuilder& builder,
                                           const type::Type* target_ty,
                                           const Source& source) const override;

    const size_t count;
};

}  // namespace tint::constant

#endif  // SRC_TINT_CONSTANT_SPLAT_H_
