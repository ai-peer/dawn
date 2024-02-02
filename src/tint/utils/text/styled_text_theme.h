// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef SRC_TINT_UTILS_TEXT_STYLED_TEXT_THEME_H_
#define SRC_TINT_UTILS_TEXT_STYLED_TEXT_THEME_H_

#include <stdint.h>
#include <optional>

/// Forward declarations
namespace tint {
class TextStyle;
}

namespace tint {

/// StyledTextTheme holds coloring information for TextStyles
struct StyledTextTheme {
    struct Color {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;

        /// Equality operator
        bool operator==(const Color& other) const {
            return r == other.r && g == other.g && b == other.b;
        }
    };

    struct Style {
        std::optional<Color> foreground;
        std::optional<Color> background;
        std::optional<bool> bold;
        std::optional<bool> code;
        std::optional<bool> italic;
        std::optional<bool> underlined;
    };

    Style From(TextStyle text_style) const;

    Style severity_success;
    Style severity_warning;
    Style severity_failure;
    Style severity_fatal;

    Style kind_variable;
    Style kind_type;
    Style kind_function;
    Style kind_enum;
    Style kind_operator;
    Style kind_squiggle;
};

}  // namespace tint

#endif  // SRC_TINT_UTILS_TEXT_STYLED_TEXT_THEME_H_
