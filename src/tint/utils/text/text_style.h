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

#ifndef SRC_TINT_UTILS_TEXT_TEXT_STYLE_H_
#define SRC_TINT_UTILS_TEXT_TEXT_STYLE_H_

#include <cstdint>
#include <optional>

namespace tint {

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    /// Equality operator
    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b;
    }
};

static constexpr Color Black = {0, 0, 0};
static constexpr Color Red = {255, 0, 0};
static constexpr Color Green = {0, 255, 0};
static constexpr Color Yellow = {255, 255, 0};
static constexpr Color Blue = {0, 0, 255};
static constexpr Color Magenta = {255, 0, 255};
static constexpr Color Cyan = {0, 255, 255};
static constexpr Color White = {255, 255, 255};

class TextStyle {
  public:
    std::optional<Color> foreground;
    std::optional<Color> background;
    bool bold = false;
    bool underline = false;

    TextStyle& Foreground(const Color& value) {
        foreground = value;
        return *this;
    }

    TextStyle& Background(const Color& value) {
        background = value;
        return *this;
    }

    TextStyle& Bold(bool value = true) {
        bold = true;
        return *this;
    }

    TextStyle& Underline(bool value = true) {
        underline = true;
        return *this;
    }

    /// Equality operator
    bool operator==(const TextStyle& other) const {
        return foreground == other.foreground && background == other.background &&
               bold == other.bold && underline == other.underline;
    }
};

}  // namespace tint

#endif  // SRC_TINT_UTILS_TEXT_TEXT_STYLE_H_
