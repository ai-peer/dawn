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

#include "src/tint/constant/splat.h"

#include "src/tint/program_builder.h"

namespace tint::constant {

Splat::~Splat() = default;

utils::Result<const Constant*> Splat::Convert(ProgramBuilder& builder,
                                              const type::Type* target_ty,
                                              const Source& source) const {
    // Convert the single splatted element type.
    auto conv_el = elements[0]->Convert(builder, type::Type::ElementOf(target_ty), source);
    if (!conv_el) {
        return utils::Failure;
    }
    if (!conv_el.Get()) {
        return nullptr;
    }
    return builder.create<constant::Splat>(target_ty, conv_el.Get(), count);
}

}  // namespace tint::constant
