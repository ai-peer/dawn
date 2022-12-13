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

#include "src/tint/constant/composite.h"

#include "src/tint/program_builder.h"

namespace tint::constant {

Composite::~Composite() = default;

utils::Result<const Constant*> Composite::Convert(ProgramBuilder& builder,
                     const type::Type* target_ty,
                     const Source& source) const {
    // Convert each of the composite element types.
    utils::Vector<const constant::Constant*, 4> conv_els;
    conv_els.Reserve(elements.Length());

    std::function<const type::Type*(size_t idx)> target_el_ty;
    if (auto* str = target_ty->As<type::Struct>()) {
        if (str->Members().Length() != elements.Length()) {
            TINT_ICE(Resolver, builder.Diagnostics())
                << "const-eval conversion of structure has mismatched element counts";
            return utils::Failure;
        }
        target_el_ty = [str](size_t idx) { return str->Members()[idx]->Type(); };
    } else {
        auto* el_ty = type::Type::ElementOf(target_ty);
        target_el_ty = [el_ty](size_t) { return el_ty; };
    }

    for (auto* el : elements) {
        auto conv_el = el->Convert(builder, target_el_ty(conv_els.Length()), source);
        if (!conv_el) {
            return utils::Failure;
        }
        if (!conv_el.Get()) {
            return nullptr;
        }
        conv_els.Push(conv_el.Get());
    }
    return builder.createCompositeOrSplat(target_ty, std::move(conv_els));
}

}  // namespace tint::constant


