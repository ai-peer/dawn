// Copyright 2023 The Tint Authors.
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

#include "src/tint/lang/glsl/writer/common/printer_support.h"

#include <cmath>
#include <limits>

#include "src/tint/utils/strconv/float_to_string.h"

namespace tint::glsl::writer {

void PrintI32(StringStream& out, int32_t value) {
    // GLSL parses `-2147483648` as a unary minus and `2147483648` as separate tokens, and the
    // latter doesn't fit into an (32-bit) `int`. Emit `(-2147483647 - 1)` instead, which ensures
    // the expression type is `int`.
    if (auto int_min = std::numeric_limits<int32_t>::min(); value == int_min) {
        out << "(" << int_min + 1 << " - 1)";
    } else {
        out << value;
    }
}

void PrintF32(StringStream& out, float value) {
    if (std::isinf(value)) {
        out << "0.0f " << (value >= 0 ? "/* inf */" : "/* -inf */");
    } else if (std::isnan(value)) {
        out << "0.0f /* nan */";
    } else {
        out << tint::writer::FloatToString(value) << "f";
    }
}

void PrintF16(StringStream& out, float value) {
    if (std::isinf(value)) {
        out << "0.0hf " << (value >= 0 ? "/* inf */" : "/* -inf */");
    } else if (std::isnan(value)) {
        out << "0.0hf /* nan */";
    } else {
        out << tint::writer::FloatToString(value) << "hf";
    }
}

}  // namespace tint::glsl::writer
