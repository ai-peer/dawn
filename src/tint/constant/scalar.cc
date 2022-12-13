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

#include "src/tint/constant/scalar.h"

#include "src/tint/program_builder.h"
#include "src/tint/type/abstract_int.h"
#include "src/tint/type/abstract_float.h"

namespace tint::constant {

template <typename T>
utils::Result<const Constant*> Scalar<T>::Convert(ProgramBuilder& builder,
                     const type::Type* target_ty,
                     const Source& source) const {
    TINT_BEGIN_DISABLE_WARNING(UNREACHABLE_CODE);
    if (target_ty == type) {
        // If the types are identical, then no conversion is needed.
        return this;
    }

    auto f = [&](auto zero_to) -> utils::Result<const constant::Constant*> {
        // `value` is the source value.
        // `FROM` is the source type.
        // `TO` is the target type.
        using TO = std::decay_t<decltype(zero_to)>;
        using FROM = T;
        if constexpr (std::is_same_v<TO, bool>) {
            // [x -> bool]
            return builder.create<constant::Scalar<TO>>(target_ty, !IsPositiveZero(value));
        } else if constexpr (std::is_same_v<FROM, bool>) {
            // [bool -> x]
            return builder.create<constant::Scalar<TO>>(target_ty, TO(value ? 1 : 0));
        } else if (auto conv = CheckedConvert<TO>(value)) {
            // Conversion success
            return builder.create<constant::Scalar<TO>>(target_ty, conv.Get());
            // --- Below this point are the failure cases ---
        } else if constexpr (IsAbstract<FROM>) {
            // [abstract-numeric -> x] - materialization failure
            builder.Diagnostics().add_error(
                tint::diag::System::Resolver,
                OverflowErrorMessage(value, builder.FriendlyName(target_ty)), source);
            return utils::Failure;
        } else if constexpr (IsFloatingPoint<TO>) {
            // [x -> floating-point] - number not exactly representable
            // https://www.w3.org/TR/WGSL/#floating-point-conversion
            builder.Diagnostics().add_error(
                tint::diag::System::Resolver,
                OverflowErrorMessage(value, builder.FriendlyName(target_ty)), source);
            return utils::Failure;
        } else if constexpr (IsFloatingPoint<FROM>) {
            // [floating-point -> integer] - number not exactly representable
            // https://www.w3.org/TR/WGSL/#floating-point-conversion
            switch (conv.Failure()) {
                case ConversionFailure::kExceedsNegativeLimit:
                    return builder.create<constant::Scalar<TO>>(target_ty, TO::Lowest());
                case ConversionFailure::kExceedsPositiveLimit:
                    return builder.create<constant::Scalar<TO>>(target_ty, TO::Highest());
            }
        } else if constexpr (IsIntegral<FROM>) {
            // [integer -> integer] - number not exactly representable
            // Static cast
            return builder.create<constant::Scalar<TO>>(target_ty, static_cast<TO>(value));
        }
        return nullptr;  // Expression is not constant.
    };

    TINT_END_DISABLE_WARNING(UNREACHABLE_CODE);

    return Switch(
        target_ty,                                                 //
        [&](const type::AbstractInt*) { return f(AInt(0)); },      //
        [&](const type::AbstractFloat*) { return f(AFloat(0)); },  //
        [&](const type::I32*) { return f(i32(0)); },               //
        [&](const type::U32*) { return f(u32(0)); },               //
        [&](const type::F32*) { return f(f32(0)); },               //
        [&](const type::F16*) { return f(f16(0)); },               //
        [&](const type::Bool*) { return f(static_cast<bool>(0)); });

}

}  // namespace tint::constant

