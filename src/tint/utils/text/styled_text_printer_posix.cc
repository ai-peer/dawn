// Copyright 2020 The Dawn & Tint Authors
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

#include <cstring>

#include "src/tint/utils/text/styled_text_printer.h"
#include "src/tint/utils/text/text_style.h"

namespace tint {
namespace {

#define ESCAPE "\u001b"

bool SupportsColors(FILE* f) {
    if (!isatty(fileno(f))) {
        return false;
    }

    const char* cterm = getenv("TERM");
    if (cterm == nullptr) {
        return false;
    }

    std::string term = getenv("TERM");
    if (term != "cygwin" && term != "linux" && term != "rxvt-unicode-256color" &&
        term != "rxvt-unicode" && term != "screen-256color" && term != "screen" &&
        term != "tmux-256color" && term != "tmux" && term != "xterm-256color" &&
        term != "xterm-color" && term != "xterm") {
        return false;
    }

    return true;
}

class PrinterPosix : public StyledTextPrinter {
  public:
    PrinterPosix(FILE* f, bool colors) : file(f), use_colors(colors && SupportsColors(f)) {}

    void Print(std::string_view text, const TextStyle& style) override {
        if (use_colors) {
            if (style.foreground.has_value()) {
                fprintf(file, ESCAPE "[38;2;%d;%d;%dm",  //
                        static_cast<int>(style.foreground->r),
                        static_cast<int>(style.foreground->g),
                        static_cast<int>(style.foreground->b));
            }
            if (style.background.has_value()) {
                fprintf(file, ESCAPE "[48;2;%d;%d;%dm",  //
                        static_cast<int>(style.background->r),
                        static_cast<int>(style.background->g),
                        static_cast<int>(style.background->b));
            }
            if (style.underline) {
                fprintf(file, ESCAPE "[4m");
            }
            if (style.bold) {
                fprintf(file, ESCAPE "[1m");
            }
            fwrite(text.data(), 1, text.size(), file);
            fprintf(file, ESCAPE "[m");

        } else {
            fwrite(text.data(), 1, text.size(), file);
        }
    }

  private:
    FILE* const file;
    const bool use_colors;
};

}  // namespace

std::unique_ptr<StyledTextPrinter> StyledTextPrinter::Create(FILE* out, bool use_colors) {
    return std::make_unique<PrinterPosix>(out, use_colors);
}

StyledTextPrinter::~StyledTextPrinter() = default;

}  // namespace tint
