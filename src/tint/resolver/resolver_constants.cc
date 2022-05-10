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

using EvaluateResult = utils::Result<sem::Constant>;
static constexpr const utils::FailureType Failure;

/// Converts all the element values of `in` to the type `T`, using the function `F`.
/// @param elements_in the vector of elements to be converted
/// @param converter a function-like with the signature `void(TO&, FROM)`
/// @returns the elements converted to type T.
template <typename T, typename ELEMENTS_IN, typename CONVERTER>
sem::Constant::Elements Transform(const ELEMENTS_IN& elements_in, CONVERTER& converter) {
    return utils::Transform(elements_in, [&](auto value_in) {
        if constexpr (std::is_same_v<UnwrapNumber<T>, bool>) {
            return AInt(value_in != 0);
        } else {
            T converted{};
            converter(converted, value_in);
            if constexpr (IsFloatingPoint<UnwrapNumber<T>>) {
                return AFloat(converted);
            } else {
                return AInt(converted);
            }
        }
    });
}

/// Converts all the element values of `in` to the semantic type `el_ty`, returning the elements
/// converted to the type `ty`. If some of the elements cannot be represented by the target type
/// then on_err is called with the element value that caused the error.
/// @param in the constant to convert
/// @param el_ty the target element type
/// @param converter a function-like with the signature `void(TO&, FROM)`
/// @returns the elements converted to `type`
template <typename CONVERTER>
sem::Constant::Elements Transform(const sem::Constant::Elements& in,
                                  const sem::Type* el_ty,
                                  CONVERTER& converter) {
    return std::visit(
        [&](auto&& v) {
            return Switch(
                el_ty,  //
                [&](const sem::AbstractInt*) { return Transform<AInt>(v, converter); },
                [&](const sem::AbstractFloat*) { return Transform<AFloat>(v, converter); },
                [&](const sem::I32*) { return Transform<i32>(v, converter); },
                [&](const sem::U32*) { return Transform<u32>(v, converter); },
                [&](const sem::F32*) { return Transform<f32>(v, converter); },
                [&](const sem::F16*) { return Transform<f16>(v, converter); },
                [&](const sem::Bool*) { return Transform<bool>(v, converter); },
                [&](Default) -> sem::Constant::Elements {
                    diag::List diags;
                    TINT_UNREACHABLE(Semantic, diags)
                        << "invalid element type " << el_ty->TypeInfo().name;
                    return {};
                });
        },
        in);
}

struct Caster {
    template <typename OUT, typename IN>
    void operator()(OUT& out, IN in) {
        out = OUT(static_cast<UnwrapNumber<OUT>>(in));
    }
};

struct Materializer {
    template <typename OUT, typename IN>
    void operator()(OUT& out, IN in) {
        if (auto conv = CheckedConvert<OUT>(in)) {
            out = conv.Get();
        } else if (!failure.has_value()) {
            std::stringstream ss;
            ss << "value " << in << " cannot be represented as '" << el_ty->FriendlyName(syms)
               << "'";
            failure = ss.str();
        }
    }

    SymbolTable& syms;
    const sem::Type* const el_ty;
    std::optional<std::string> failure{};
};

sem::Constant::Elements CastElements(const sem::Constant::Elements& in, const sem::Type* el_ty) {
    Caster conv{};
    return Transform(in, el_ty, conv);
}

utils::Result<sem::Constant::Elements> MaterializeElements(const sem::Constant::Elements& in,
                                                           const sem::Type* el_ty,
                                                           ProgramBuilder& builder,
                                                           Source source) {
    Materializer conv{builder.Symbols(), el_ty};
    auto out = Transform(in, el_ty, conv);
    if (conv.failure.has_value()) {
        builder.Diagnostics().add_error(diag::System::Resolver, std::move(conv.failure.value()),
                                        source);
        return Failure;
    }
    return out;
}

}  // namespace

EvaluateResult Resolver::EvaluateConstantValue(const ast::Expression* expr, const sem::Type* type) {
    if (auto* e = expr->As<ast::LiteralExpression>()) {
        return EvaluateConstantValue(e, type);
    }
    if (auto* e = expr->As<ast::CallExpression>()) {
        return EvaluateConstantValue(e, type);
    }
    return sem::Constant{};
}

EvaluateResult Resolver::EvaluateConstantValue(const ast::LiteralExpression* literal,
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

EvaluateResult Resolver::EvaluateConstantValue(const ast::CallExpression* call,
                                               const sem::Type* ty) {
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

    // Build value for type_ctor from each child value by converting to type_ctor's type.
    std::optional<sem::Constant::Elements> elements;
    for (auto* expr : call->args) {
        auto* arg = builder_->Sem().Get(expr);
        if (!arg) {
            return sem::Constant{};
        }
        auto value = arg->ConstantValue();
        if (!value) {
            return sem::Constant{};
        }

        // Conv the elements to the desired type.
        auto converted = CastElements(value.GetElements(), el_ty);

        if (elements.has_value()) {
            // Append the converted vector to elements
            std::visit(
                [&](auto&& dst) {
                    using VEC_TY = std::decay_t<decltype(dst)>;
                    const auto& src = std::get<VEC_TY>(converted);
                    dst.insert(dst.end(), src.begin(), src.end());
                },
                elements.value());
        } else {
            elements = std::move(converted);
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
        elements.value());

    return sem::Constant(ty, std::move(elements.value()));
}

EvaluateResult Resolver::ConvertValue(const sem::Constant& value,
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
        return sem::Constant(ty, value.GetElements());
    }

    auto res = MaterializeElements(value.GetElements(), el_ty, *builder_, source);
    if (!res) {
        return Failure;
    }
    return sem::Constant(ty, std::move(res.Get()));
}

}  // namespace tint::resolver
