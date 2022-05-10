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

#ifndef SRC_TINT_UTILS_RESULT_H_
#define SRC_TINT_UTILS_RESULT_H_

#include <variant>

namespace tint::utils {

/// Empty structure used as the default ERROR_TYPE for a Result.
struct FailureType {};

static constexpr const FailureType Failure;

template <typename SUCCESS_TYPE, typename ERROR_TYPE = FailureType>
struct Result {
    Result(const SUCCESS_TYPE& success) : value{success} {}
    Result(const ERROR_TYPE& error) : value{error} {}

    bool operator!() const { return std::holds_alternative<ERROR_TYPE>(value); }

    const SUCCESS_TYPE* operator->() const { return &std::get<SUCCESS_TYPE>(value); }
    const SUCCESS_TYPE& Get() const { return std::get<SUCCESS_TYPE>(value); }

    std::variant<SUCCESS_TYPE, ERROR_TYPE> value;
};

}  // namespace tint::utils

#endif  // SRC_TINT_UTILS_RESULT_H_
