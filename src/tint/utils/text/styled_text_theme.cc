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

#include "src/tint/utils/text/styled_text_theme.h"
#include "src/tint/utils/text/text_style.h"

namespace tint {

StyledTextTheme::Style StyledTextTheme::From(TextStyle text_style) const {
    Style out;
    auto apply = [&](const Style& in) {
        if (in.foreground) {
            out.foreground = in.foreground;
        }
        if (in.background) {
            out.background = in.background;
        }
        if (in.bold) {
            out.bold = in.bold;
        }
        if (in.code) {
            out.code = in.code;
        }
        if (in.italic) {
            out.italic = in.italic;
        }
        if (in.underlined) {
            out.underlined = in.underlined;
        }
    };

    if (text_style.IsSuccess()) {
        apply(severity_success);
    } else if (text_style.IsWarning()) {
        apply(severity_warning);
    } else if (text_style.IsError()) {
        apply(severity_failure);
    } else if (text_style.IsFatal()) {
        apply(severity_fatal);
    }

    if (text_style.IsVariable()) {
        apply(kind_variable);
    } else if (text_style.IsType()) {
        apply(kind_type);
    } else if (text_style.IsFunction()) {
        apply(kind_function);
    } else if (text_style.IsEnum()) {
        apply(kind_enum);
    } else if (text_style.IsOperator()) {
        apply(kind_operator);
    } else if (text_style.IsSquiggle()) {
        apply(kind_squiggle);
    }

    if (text_style.IsBold()) {
        out.bold = true;
    }
    if (text_style.IsItalic()) {
        out.italic = true;
    }
    if (text_style.IsUnderlined()) {
        out.underlined = true;
    }

    return out;
}

}  // namespace tint
