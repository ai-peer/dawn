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

#include "src/tint/sem/load.h"

#include "src/tint/debug.h"
#include "src/tint/sem/reference.h"

TINT_INSTANTIATE_TYPEINFO(tint::sem::Load);

namespace tint::sem {
Load::Load(const Expression* ref, const Statement* statement, const Constant* constant)
    : Base(/* declaration */ ref->Declaration(),
           /* type */ constant->Type(),
           /* stage */ EvaluationStage::kConstant,  // Abstract can only be const-expr
           /* statement */ statement,
           /* constant */ constant,
           /* has_side_effects */ false,
           /* root_ident */ ref->RootIdentifier()),
      reference_(ref) {
    TINT_ASSERT(Semantic, ref->Type()->Is<sem::Reference>());
}

Load::~Load() = default;

}  // namespace tint::sem
