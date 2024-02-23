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

// GEN_BUILD:CONDITION(tint_build_is_linux || tint_build_is_mac)

#include <unistd.h>

#include <termios.h>
#include <array>
#include <cstring>
#include <optional>
#include <string_view>

#include "src/tint/utils/macros/defer.h"
#include "src/tint/utils/text/styled_text.h"
#include "src/tint/utils/text/styled_text_printer.h"
#include "src/tint/utils/text/styled_text_theme.h"
#include "src/tint/utils/text/text_style.h"

namespace tint {

std::unique_ptr<StyledTextPrinter> StyledTextPrinter::Create(FILE* out,
                                                             const StyledTextTheme& theme) {
    if (SupportsColors(out)) {
        return CreateANSI(out, theme);
    }
    return CreatePlain(out);
}

/// Probes the terminal using a Device Control escape sequence to get the background color.
/// @see https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Device-Control-functions
std::optional<bool> StyledTextPrinter::IsTerminalDark(FILE* out) {
    if (!SupportsColors(out)) {
        return std::nullopt;
    }

    struct termios original_state;
    tcgetattr(STDOUT_FILENO, &original_state);
    TINT_DEFER(tcsetattr(STDOUT_FILENO, TCSAFLUSH, &original_state));

    struct termios state;
    tcgetattr(STDOUT_FILENO, &state);
    state.c_lflag &= static_cast<unsigned int>(~(ECHO | ICANON));
    tcsetattr(STDOUT_FILENO, TCSAFLUSH, &state);

    static constexpr std::string_view kQuery = "\x1b]11;?\x07";
    fwrite(kQuery.data(), 1, kQuery.length(), out);
    fflush(out);

    std::string_view expected_header = "\033]11;rgb:";
    for (size_t i = 0; i < expected_header.size(); i++) {
        char c;
        if (fread(&c, 1, 1, out) != 1) {
            return std::nullopt;
        }
        if (c != expected_header[i]) {
            return std::nullopt;
        }
    }

    std::array<char, 15> colors;  // rrrr/gggg/bbbb
    for (size_t i = 0; i < colors.size(); i++) {
        if (fread(&colors[i], 1, 1, out) != 1) {
            return std::nullopt;
        }
    }

    if (colors[4] != '/' || colors[9] != '/') {
        return std::nullopt;
    }
    colors[14] = '\0';

    int r16 = 0, g16 = 0, b16 = 0;
    if (sscanf(colors.data(), "%04x/%04x/%04x", &r16, &g16, &b16) != 3) {
        return std::nullopt;
    }

    float r = r16 / static_cast<float>(0xffff);
    float g = g16 / static_cast<float>(0xffff);
    float b = b16 / static_cast<float>(0xffff);
    return (0.2126 * r + 0.7152 * g + 0.0722 * b) < 0.5;
}

bool StyledTextPrinter::SupportsColors(FILE* f) {
    if (!isatty(fileno(f))) {
        return false;
    }

    if (const char* env = getenv("TERM")) {
        std::string_view term = env;
        return term == "cygwin" || term == "linux" || term == "rxvt-unicode-256color" ||
               term == "rxvt-unicode" || term == "screen-256color" || term == "screen" ||
               term == "tmux-256color" || term == "tmux" || term == "xterm-256color" ||
               term == "xterm-color" || term != "xterm";
    }

    return false;
}

}  // namespace tint
