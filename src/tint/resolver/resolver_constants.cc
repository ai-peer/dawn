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

#include <cmath>
#include <optional>

#include "src/tint/sem/abstract_float.h"
#include "src/tint/sem/abstract_int.h"
#include "src/tint/sem/constant.h"
#include "src/tint/sem/type_constructor.h"
#include "src/tint/utils/compiler_macros.h"
#include "src/tint/utils/map.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::resolver {

namespace {

template <typename V, typename F>
auto Cast(const sem::Type* type, V value, F&& f) {
    return Switch(
        type,                                                         //
        [&](const sem::AbstractInt*) { return f(AInt(value)); },      //
        [&](const sem::AbstractFloat*) { return f(AFloat(value)); },  //
        [&](const sem::I32*) { return f(i32(value)); },               //
        [&](const sem::U32*) { return f(u32(value)); },               //
        [&](const sem::F32*) { return f(f32(value)); },               //
        [&](const sem::F16*) { return f(f16(value)); },               //
        [&](const sem::Bool*) { return f(bool(value)); });
}

template <typename T>
inline bool IsNegativeFloat(T value) {
    (void)value;
    if constexpr (IsFloatingPoint<T>) {
        return std::signbit(value);
    } else {
        return false;
    }
}

template <typename T>
inline auto ValueOf(T value) {
    if constexpr (std::is_same_v<UnwrapNumber<T>, T>) {
        return value;
    } else {
        return value.value;
    }
}

template <typename T>
inline bool IsPositiveZero(T value) {
    using N = UnwrapNumber<T>;
    return std::equal_to<N>()(ValueOf(value), static_cast<N>(0)) &&
           !IsNegativeFloat(ValueOf(value));
}

struct Constant : public sem::Constant {
    virtual utils::Result<const Constant*> Convert(ProgramBuilder& builder,
                                                   const sem::Type* target_ty,
                                                   const Source& source) const = 0;
};

// Forward declaration
const Constant* CreateComposite(ProgramBuilder& builder,
                                const sem::Type* type,
                                std::vector<const Constant*> elements);

template <typename T>
struct Element : Constant {
    Element(const sem::Type* t, T v) : type(t), value(v) {}
    ~Element() override = default;
    const sem::Type* Type() const override { return type; }
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
                                           const sem::Type* target_ty,
                                           const Source& source) const override {
        if (target_ty == type) {
            return this;
        }
        if constexpr (std::is_same_v<T, AInt> || std::is_same_v<T, AFloat>) {
            auto* res = Cast(target_ty, 0, [&](auto used_for_static_type) -> const Constant* {
                using TO = std::decay_t<decltype(used_for_static_type)>;
                if constexpr (std::is_same_v<TO, bool>) {
                    return builder.create<Element<TO>>(target_ty, IsPositiveZero(value));
                } else if (auto conv = CheckedConvert<TO>(value)) {
                    return builder.create<Element<TO>>(target_ty, conv.Get());
                }
                return nullptr;
            });
            if (!res) {
                std::stringstream ss;
                ss << "value " << value << " cannot be represented as ";
                ss << "'" << builder.FriendlyName(target_ty) << "'";
                builder.Diagnostics().add_error(tint::diag::System::Resolver, ss.str(), source);
                return utils::Failure;
            }
            return res;
        } else {
            return Cast(target_ty, 0, [&](auto used_for_static_type) -> const Constant* {
                using TO = std::decay_t<decltype(used_for_static_type)>;
                if constexpr (std::is_same_v<TO, T>) {
                    return this;
                } else if constexpr (std::is_same_v<TO, bool>) {  // x -> bool
                    return builder.create<Element<TO>>(target_ty, !IsPositiveZero(value));
                } else if constexpr (std::is_same_v<T, bool>) {  // bool -> x
                    return builder.create<Element<TO>>(target_ty, TO(value ? 1 : 0));
                } else if (auto conv = CheckedConvert<TO>(value)) {
                    return builder.create<Element<TO>>(target_ty, conv.Get());
                } else {
                    if constexpr (IsFloatingPoint<UnwrapNumber<TO>>) {
                        constexpr auto kInf = std::numeric_limits<double>::infinity();
                        switch (conv.Failure()) {
                            case ConversionFailure::kExceedsNegativeLimit:
                                return builder.create<Element<TO>>(target_ty, TO(-kInf));
                            case ConversionFailure::kExceedsPositiveLimit:
                                return builder.create<Element<TO>>(target_ty, TO(kInf));
                        }
                    } else {
                        switch (conv.Failure()) {
                            case ConversionFailure::kExceedsNegativeLimit:
                                return builder.create<Element<TO>>(target_ty, TO(TO::kLowest));
                            case ConversionFailure::kExceedsPositiveLimit:
                                return builder.create<Element<TO>>(target_ty, TO(TO::kHighest));
                        }
                    }
                }
                return nullptr;
            });
        }
    }

    sem::Type const* const type;
    const T value;
};

struct Splat : Constant {
    Splat(const sem::Type* t, const Constant* e, size_t n) : type(t), el(e), count(n) {
        TINT_ASSERT(Resolver, el != nullptr);
    }
    ~Splat() override = default;
    const sem::Type* Type() const override { return type; }
    std::variant<std::monostate, AInt, AFloat> Value() const override { return {}; }
    const Constant* Index(size_t i) const override { return i < count ? el : nullptr; }
    bool AllZero() const override { return el->AllZero(); }
    bool AnyZero() const override { return el->AnyZero(); }
    bool AllEqual() const override { return true; }
    size_t Hash() const override { return utils::Hash(type, el->Hash(), count); }

    utils::Result<const Constant*> Convert(ProgramBuilder& builder,
                                           const sem::Type* target_ty,
                                           const Source& source) const override {
        auto conv_el = el->Convert(builder, sem::Type::ElementOf(target_ty), source);
        if (!conv_el) {
            return utils::Failure;
        }
        if (!conv_el.Get()) {
            return nullptr;
        }
        return builder.create<Splat>(target_ty, conv_el.Get(), count);
    }

    sem::Type const* const type;
    const Constant* el;
    const size_t count;
};

struct Composite : Constant {
    Composite(const sem::Type* t, std::vector<const Constant*> els, bool all_0, bool any_0)
        : type(t), elements(std::move(els)), all_zero(all_0), any_zero(any_0), hash(CalcHash()) {}
    ~Composite() override = default;
    const sem::Type* Type() const override { return type; }
    std::variant<std::monostate, AInt, AFloat> Value() const override { return {}; }
    const Constant* Index(size_t i) const override {
        return i < elements.size() ? elements[i] : nullptr;
    }
    bool AllZero() const override { return all_zero; }
    bool AnyZero() const override { return any_zero; }
    bool AllEqual() const override { return false; /* otherwise this should be a Splat */ }
    size_t Hash() const override { return hash; }

    utils::Result<const Constant*> Convert(ProgramBuilder& builder,
                                           const sem::Type* target_ty,
                                           const Source& source) const override {
        auto* el_ty = sem::Type::ElementOf(target_ty);
        std::vector<const Constant*> conv_els;
        conv_els.reserve(elements.size());
        for (auto* el : elements) {
            auto conv_el = el->Convert(builder, el_ty, source);
            if (!conv_el) {
                return utils::Failure;
            }
            if (!conv_el.Get()) {
                return nullptr;
            }
            conv_els.emplace_back(conv_el.Get());
        }
        return CreateComposite(builder, target_ty, std::move(conv_els));
    }

    size_t CalcHash() {
        auto h = utils::Hash(type, all_zero, any_zero);
        for (auto* el : elements) {
            utils::HashCombine(&h, el->Hash());
        }
        return h;
    }

    sem::Type const* const type;
    const std::vector<const Constant*> elements;
    const bool all_zero;
    const bool any_zero;
    const size_t hash;
};

template <typename T>
const Constant* CreateElement(ProgramBuilder& builder, const sem::Type* t, T v) {
    return builder.create<Element<T>>(t, v);
}

const Constant* ZeroValue(ProgramBuilder& builder, const sem::Type* type) {
    return Switch(
        type,  //
        [&](const sem::Vector* v) -> const Constant* {
            auto* zero_el = ZeroValue(builder, v->type());
            return builder.create<Splat>(type, zero_el, v->Width());
        },
        [&](const sem::Matrix* m) -> const Constant* {
            auto* zero_el = ZeroValue(builder, m->ColumnType());
            return builder.create<Splat>(type, zero_el, m->columns());
        },
        [&](const sem::Array* a) -> const Constant* {
            if (auto* zero_el = ZeroValue(builder, a->ElemType())) {
                return builder.create<Splat>(type, zero_el, a->Count());
            }
            return nullptr;
        },
        [&](Default) -> const Constant* {
            return Cast(type, 0, [&](auto zero) -> const Constant* {
                return CreateElement(builder, type, zero);
            });
        });
}

bool Equal(const sem::Constant* a, const sem::Constant* b) {
    if (a->Hash() != b->Hash()) {
        return false;
    }
    if (a->Type() != b->Type()) {
        return false;
    }
    return Switch(
        a->Type(),  //
        [&](const sem::Vector* vec) {
            for (size_t i = 0; i < vec->Width(); i++) {
                if (!Equal(a->Index(i), b->Index(i))) {
                    return false;
                }
            }
            return true;
        },
        [&](const sem::Matrix* mat) {
            for (size_t i = 0; i < mat->columns(); i++) {
                if (!Equal(a->Index(i), b->Index(i))) {
                    return false;
                }
            }
            return true;
        },
        [&](const sem::Array* arr) {
            for (size_t i = 0; i < arr->Count(); i++) {
                if (!Equal(a->Index(i), b->Index(i))) {
                    return false;
                }
            }
            return true;
        },
        [&](Default) { return a->Value() == b->Value(); });
}

const Constant* CreateComposite(ProgramBuilder& builder,
                                const sem::Type* type,
                                std::vector<const Constant*> elements) {
    if (elements.size() == 0) {
        return nullptr;
    }
    bool any_zero = false;
    bool all_zero = true;
    bool all_equal = true;
    auto* first = elements.front();
    for (auto* el : elements) {
        if (!any_zero && el->AnyZero()) {
            any_zero = true;
        }
        if (all_zero && !el->AllZero()) {
            all_zero = false;
        }
        if (all_equal && el != first) {
            if (!Equal(el, first)) {
                all_equal = false;
            }
        }
    }
    if (all_equal) {
        return builder.create<Splat>(type, elements[0], elements.size());
    } else {
        return builder.create<Composite>(type, std::move(elements), all_zero, any_zero);
    }
}

}  // namespace

const sem::Constant* Resolver::EvaluateConstantValue(const ast::Expression* expr,
                                                     const sem::Type* type) {
    return Switch(
        expr,  //
        [&](const ast::IdentifierExpression* e) { return EvaluateConstantValue(e, type); },
        [&](const ast::LiteralExpression* e) { return EvaluateConstantValue(e, type); },
        [&](const ast::CallExpression* e) { return EvaluateConstantValue(e, type); },
        [&](const ast::IndexAccessorExpression* e) { return EvaluateConstantValue(e, type); });
}

const sem::Constant* Resolver::EvaluateConstantValue(const ast::IdentifierExpression* ident,
                                                     const sem::Type*) {
    if (auto* sem = builder_->Sem().Get(ident)) {
        return sem->ConstantValue();
    }
    return {};
}

const sem::Constant* Resolver::EvaluateConstantValue(const ast::LiteralExpression* literal,
                                                     const sem::Type* type) {
    return Switch(
        literal,
        [&](const ast::BoolLiteralExpression* lit) {
            return CreateElement(*builder_, type, lit->value);
        },
        [&](const ast::IntLiteralExpression* lit) -> const Constant* {
            switch (lit->suffix) {
                case ast::IntLiteralExpression::Suffix::kNone:
                    return CreateElement(*builder_, type, AInt(lit->value));
                case ast::IntLiteralExpression::Suffix::kI:
                    return CreateElement(*builder_, type, i32(lit->value));
                case ast::IntLiteralExpression::Suffix::kU:
                    return CreateElement(*builder_, type, u32(lit->value));
            }
            return nullptr;
        },
        [&](const ast::FloatLiteralExpression* lit) -> const Constant* {
            switch (lit->suffix) {
                case ast::FloatLiteralExpression::Suffix::kNone:
                    return CreateElement(*builder_, type, AFloat(lit->value));
                case ast::FloatLiteralExpression::Suffix::kF:
                    return CreateElement(*builder_, type, f32(lit->value));
                case ast::FloatLiteralExpression::Suffix::kH:
                    return CreateElement(*builder_, type, f16(lit->value));
            }
            return nullptr;
        });
}

const sem::Constant* Resolver::EvaluateConstantValue(const ast::CallExpression* call,
                                                     const sem::Type* ty) {
    // Note: we are building constant values for array types. The working group as verbally agreed
    // to support constant expression arrays, but this is not (yet) part of the spec.
    // See: https://github.com/gpuweb/gpuweb/issues/3056

    // For zero value init, return 0s
    if (call->args.empty()) {
        return ZeroValue(*builder_, ty);
    }

    uint32_t el_count = 0;
    auto* el_ty = sem::Type::ElementOf(ty, &el_count);
    if (!el_ty) {
        return nullptr;  // Target type does not support constant values
    }

    auto value_of = [&](const ast::Expression* expr) {
        return static_cast<const Constant*>(builder_->Sem().Get(expr)->ConstantValue());
    };

    if (call->args.size() == 1) {
        auto& src = call->args[0]->source;
        auto* arg = value_of(call->args[0]);
        if (!arg) {
            return nullptr;
        }

        if (ty->is_scalar()) {  // Scalar type conversion: i32(x), u32(x), bool(x)
            return ConvertValue(arg, el_ty, src).Get();
        }

        if (arg->Type() == el_ty) {
            // Splat
            auto splat = [&](size_t n) { return builder_->create<Splat>(ty, arg, n); };
            return Switch(
                ty,  //
                [&](const sem::Vector* v) { return splat(v->Width()); },
                [&](const sem::Matrix* m) { return splat(m->columns()); },
                [&](const sem::Array* a) { return splat(a->Count()); });
        }

        // Conversion
        if (auto conv = ConvertValue(arg, ty, src)) {
            return conv.Get();
        }

        return nullptr;
    }

    std::vector<const Constant*> els;
    els.reserve(el_count);

    auto push_all_args = [&] {
        for (auto* expr : call->args) {
            auto* arg = value_of(expr);
            if (!arg) {
                return;
            }
            els.emplace_back(arg);
        }
    };

    Switch(
        ty,  //
        [&](const sem::Vector*) {
            // Vector can be constructed with a mix of scalars and smaller vectors.
            for (auto* expr : call->args) {
                auto* arg = value_of(expr);
                if (!arg) {
                    return;
                }
                auto* arg_ty = arg->Type();
                if (auto* arg_vec = arg_ty->As<sem::Vector>()) {
                    // Extract out vector elements.
                    for (uint32_t i = 0; i < arg_vec->Width(); i++) {
                        auto* el = static_cast<const Constant*>(arg->Index(i));
                        if (!el) {
                            return;
                        }
                        els.emplace_back(el);
                    }
                } else {
                    els.emplace_back(arg);
                }
            }
        },
        [&](const sem::Matrix* m) {
            if (call->args.size() == m->columns() * m->rows()) {
                // Matrix built from scalars
                for (uint32_t c = 0; c < m->columns(); c++) {
                    std::vector<const Constant*> column;
                    column.reserve(m->rows());
                    for (uint32_t r = 0; r < m->rows(); r++) {
                        auto* arg = value_of(call->args[r + c * m->rows()]);
                        if (!arg) {
                            return;
                        }
                        column.emplace_back(arg);
                    }
                    els.push_back(CreateComposite(*builder_, m->ColumnType(), std::move(column)));
                }
            } else if (call->args.size() == m->columns()) {
                // Matrix built from column vectors
                push_all_args();
            }
        },
        [&](const sem::Array*) { push_all_args(); });

    if (els.size() != el_count) {
        return nullptr;
    }
    return CreateComposite(*builder_, ty, std::move(els));
}

const sem::Constant* Resolver::EvaluateConstantValue(const ast::IndexAccessorExpression* accessor,
                                                     const sem::Type*) {
    auto* obj_sem = builder_->Sem().Get(accessor->object);
    if (!obj_sem) {
        return {};
    }

    auto obj_val = obj_sem->ConstantValue();
    if (!obj_val) {
        return {};
    }

    auto* idx_sem = builder_->Sem().Get(accessor->index);
    if (!idx_sem) {
        return {};
    }

    auto idx_val = idx_sem->ConstantValue();
    if (!idx_val) {
        return {};
    }

    uint32_t el_count = 0;
    sem::Type::ElementOf(obj_val->Type(), &el_count);

    AInt idx = idx_val->As<AInt>();
    if (idx < 0 || idx >= el_count) {
        auto clamped = std::min<AInt::type>(std::max<AInt::type>(idx, 0), el_count - 1);
        AddWarning("index " + std::to_string(idx) + " out of bounds [0.." +
                       std::to_string(el_count - 1) + "]. Clamping index to " +
                       std::to_string(clamped),
                   accessor->index->source);
        idx = clamped;
    }

    return obj_val->Index(idx);
}

utils::Result<const sem::Constant*> Resolver::ConvertValue(const sem::Constant* value,
                                                           const sem::Type* target_ty,
                                                           const Source& source) {
    if (value->Type() == target_ty) {
        return value;
    }
    auto conv = static_cast<const Constant*>(value)->Convert(*builder_, target_ty, source);
    if (!conv) {
        return utils::Failure;
    }
    return conv.Get();
}

}  // namespace tint::resolver
