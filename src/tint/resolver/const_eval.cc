// Copyright 2022 The Tint Authors.
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

#include "src/tint/resolver/const_eval.h"

#include "src/tint/sem/constant.h"

namespace tint::resolver::const_eval {

namespace {

bool CheckNumArgs(ProgramBuilder& b, size_t num_args, size_t count) {
    if (num_args != 2) {
        TINT_ICE(Resolver, b.Diagnostics())
            << "unexpected number of arguments for constant evaluation function.\nExpected "
            << count << ", got " << num_args;
        return false;
    }
    return true;
}

template <typename F>
sem::Constant Binary(ProgramBuilder& b, const sem::Constant* args, size_t num_args, const F&& f) {
    if (!CheckNumArgs(b, num_args, 2)) {
        return {};
    }
    return args[0].WithElements([&](auto&& lhs) {
        auto& rhs = std::get<std::decay_t<decltype(lhs)>>(args[1].GetElements());
        return f(lhs, rhs);
    });
}

template <typename T, typename F>
sem::Constant ElementWise(const sem::Type* ty,
                          const std::vector<T>& vec_a,
                          const std::vector<T>& vec_b,
                          F&& f) {
    size_t n = std::min(vec_a.size(), vec_b.size());
    std::vector<T> out(n);
    for (size_t i = 0; i < n; i++) {
        out[i] = f(vec_a[i], vec_b[i]);
    }
    return sem::Constant(ty, std::move(out));
}

}  // namespace

sem::Constant max(ProgramBuilder& builder, const sem::Constant* args, size_t num_args) {
    return Binary(builder, args, num_args, [&](auto&& vec_a, auto&& vec_b) {
        return ElementWise(args[0].Type(), vec_a, vec_b,
                           [&](auto&& a, auto&& b) { return std::max(a.value, b.value); });
    });
}

}  // namespace tint::resolver::const_eval
