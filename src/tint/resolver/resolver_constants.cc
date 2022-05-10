// Copyright 2021 The Tint Authors.
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

#include "src/tint/resolver/resolver.h"

#include "src/tint/sem/abstract_float.h"
#include "src/tint/sem/abstract_int.h"
#include "src/tint/sem/constant.h"
#include "src/tint/sem/type_constructor.h"
#include "src/tint/utils/map.h"
#include "src/tint/utils/transform.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::resolver {

namespace {

using Result = utils::Result<sem::Constant>;
static constexpr const utils::FailureType Failure;

/// Casts all the scalar values of `in` to the element type `T`.
/// If some of the scalars cannot be represented by the target type then on_err is called with
/// the scalar value that caused the error.
/// @param scalars_in the vector of scalars to cast
/// @param on_err a function-like with the signature `void(T bad_value)`
/// @returns the scalars cast to type T.
template <typename T, typename SCALARS_IN, typename ON_ERR>
sem::Constant::Scalars Cast(const SCALARS_IN& scalars_in, ON_ERR&& on_err) {
    using E = UnwrapNumber<T>;
    return utils::Transform(scalars_in, [&](auto value_in) {
        if constexpr (std::is_same_v<E, bool>) {
            return AInt(value_in != 0);
        }

        E cast = static_cast<E>(value_in);
        if (value_in != cast) {
            // TODO(crbug.com/tint/1504): Handle FP comparisons.
            on_err(value_in);
        }
        if constexpr (std::is_floating_point_v<E>) {
            return AFloat(cast);
        } else {
            return AInt(cast);
        }
    });
}

/// Casts all the scalar values of `in` to the semantic type `el_ty`, returning the scalars cast to
/// the type `ty`. If some of the scalars cannot be represented by the target type then on_err is
/// called with the scalar value that caused the error.
/// @param in the constant to cast
/// @param el_ty the target scalar element type
/// @param on_err a function-like with the signature `void(T bad_value)`
/// @returns the scalars cast to `type`
template <typename ON_ERR>
sem::Constant::Scalars Cast(const sem::Constant::Scalars& in,
                            const sem::Type* el_ty,
                            ON_ERR&& on_err) {
    return std::visit(
        [&](auto&& v) {
            return Switch(
                el_ty,  //
                [&](const sem::AbstractInt*) {
                    return Cast<AInt>(v, std::forward<ON_ERR>(on_err));
                },
                [&](const sem::AbstractFloat*) {
                    return Cast<AFloat>(v, std::forward<ON_ERR>(on_err));
                },
                [&](const sem::I32*) { return Cast<i32>(v, std::forward<ON_ERR>(on_err)); },
                [&](const sem::U32*) { return Cast<u32>(v, std::forward<ON_ERR>(on_err)); },
                [&](const sem::F32*) { return Cast<f32>(v, std::forward<ON_ERR>(on_err)); },
                [&](const sem::F16*) { return Cast<f16>(v, std::forward<ON_ERR>(on_err)); },
                [&](const sem::Bool*) { return Cast<bool>(v, std::forward<ON_ERR>(on_err)); },
                [&](Default) -> sem::Constant::Scalars {
                    diag::List diags;
                    TINT_UNREACHABLE(Semantic, diags)
                        << "invalid element type " << el_ty->TypeInfo().name;
                    return {};
                });
        },
        in);
}

}  // namespace

Result Resolver::EvaluateConstantValue(const ast::Expression* expr, const sem::Type* type) {
    if (auto* e = expr->As<ast::LiteralExpression>()) {
        return EvaluateConstantValue(e, type);
    }
    if (auto* e = expr->As<ast::CallExpression>()) {
        return EvaluateConstantValue(e, type);
    }
    return sem::Constant{};
}

Result Resolver::EvaluateConstantValue(const ast::LiteralExpression* literal,
                                       const sem::Type* type) {
    return Switch(
        literal,
        [&](const ast::BoolLiteralExpression* lit) {
            return sem::Constant{type, {AInt(lit->value ? 1 : 0)}};
        },
        [&](const ast::IntLiteralExpression* lit) {
            return sem::Constant{type, {AInt(lit->value)}};
        },
        [&](const ast::FloatLiteralExpression* lit) {
            return sem::Constant{type, {AFloat(lit->value)}};
        });
}

Result Resolver::EvaluateConstantValue(const ast::CallExpression* call, const sem::Type* ty) {
    uint32_t result_size = 0;
    auto* el_ty = sem::Type::ElementOf(ty, &result_size);
    if (!el_ty) {
        return sem::Constant{};
    }

    // ElementOf() will also return the element type of array, which we do not support.
    if (ty->Is<sem::Array>()) {
        return sem::Constant{};
    }

    // For zero value init, return 0s
    if (call->args.empty()) {
        return Switch(
            el_ty,
            [&](const sem::AbstractInt*) {
                return sem::Constant(ty, std::vector<AInt>(result_size, AInt(0)));
            },
            [&](const sem::AbstractFloat*) {
                return sem::Constant(ty, std::vector<AFloat>(result_size, AFloat(0)));
            },
            [&](const sem::I32*) {
                return sem::Constant(ty, std::vector<AInt>(result_size, AInt(0)));
            },
            [&](const sem::U32*) {
                return sem::Constant(ty, std::vector<AInt>(result_size, AInt(0)));
            },
            [&](const sem::F32*) {
                return sem::Constant(ty, std::vector<AFloat>(result_size, AFloat(0)));
            },
            [&](const sem::F16*) {
                return sem::Constant(ty, std::vector<AFloat>(result_size, AFloat(0)));
            },
            [&](const sem::Bool*) {
                return sem::Constant(ty, std::vector<AInt>(result_size, AInt(0)));
            });
    }

    // Build value for type_ctor from each child value by casting to type_ctor's type.
    std::optional<sem::Constant::Scalars> scalars;
    for (auto* expr : call->args) {
        auto* arg = builder_->Sem().Get(expr);
        if (!arg) {
            return sem::Constant{};
        }
        auto value = arg->ConstantValue();
        if (!value) {
            return sem::Constant{};
        }

        // Cast the elements to the desired type.
        const bool is_materialize =
            value.ElementType()->Is<sem::AbstractNumeric>() && !el_ty->Is<sem::AbstractNumeric>();
        bool ok = true;
        auto cast = Cast(value.Elements(), el_ty, [&](auto v) {
            if (is_materialize) {
                std::stringstream ss;
                ss << "value " << v << " cannot be represented as "
                   << builder_->FriendlyName(el_ty);
                AddError(ss.str(), expr->source);
                ok = false;
            }
        });
        if (!ok) {
            return Failure;
        }

        if (scalars.has_value()) {
            // Append the cast vector to scalars
            std::visit(
                [&](auto&& dst) {
                    using VEC_TY = std::decay_t<decltype(dst)>;
                    const auto& src = std::get<VEC_TY>(cast);
                    dst.insert(dst.end(), src.begin(), src.end());
                },
                scalars.value());
        } else {
            scalars = std::move(cast);
        }
    }

    // Splat single-value initializers
    std::visit(
        [&](auto&& v) {
            if (v.size() == 1) {
                for (uint32_t i = 0; i < result_size - 1; ++i) {
                    v.emplace_back(v[0]);
                }
            }
        },
        scalars.value());

    return sem::Constant(ty, std::move(scalars.value()));
}

Result Resolver::ConstantCast(const sem::Constant& value,
                              const sem::Type* ty,
                              const Source& source) {
    if (value.Type() == ty) {
        return value;
    }

    auto* el_ty = sem::Type::ElementOf(ty);
    if (el_ty == nullptr) {
        return sem::Constant{};
    }
    if (value.ElementType() == el_ty) {
        return sem::Constant(ty, value.Elements());
    }

    bool ok = true;
    auto scalars = Cast(value.Elements(), el_ty, [&](auto v) {
        std::stringstream ss;
        ss << "value " << v << " cannot be represented as " << builder_->FriendlyName(el_ty);
        AddError(ss.str(), source);
        ok = false;
    });
    if (!ok) {
        return Failure;
    }

    return sem::Constant(ty, std::move(scalars));
}

}  // namespace tint::resolver
