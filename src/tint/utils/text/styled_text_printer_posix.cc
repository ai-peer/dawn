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

#include "src/tint/utils/text/styled_text.h"
#include "src/tint/utils/text/styled_text_printer.h"
#include "src/tint/utils/text/styled_text_theme.h"
#include "src/tint/utils/text/text_style.h"

namespace tint {
namespace {

#define ESCAPE "\u001b"

bool SupportsTheme(FILE* f) {
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
    explicit PrinterPosix(FILE* f) : file_(f) {}

    void SetTheme(const StyledTextTheme* theme) override {
        if (SupportsTheme(file_)) {
            theme_ = theme;
        }
    }

    void Print(const StyledText& style_text) override {
        if (theme_) {
            style_text.Walk([&](std::string_view text, TextStyle text_style) {
                auto style = theme_->From(text_style);
                if (style.foreground.has_value()) {
                    fprintf(file_, ESCAPE "[38;2;%d;%d;%dm",  //
                            static_cast<int>(style.foreground->r),
                            static_cast<int>(style.foreground->g),
                            static_cast<int>(style.foreground->b));
                }
                if (style.background.has_value()) {
                    fprintf(file_, ESCAPE "[48;2;%d;%d;%dm",  //
                            static_cast<int>(style.background->r),
                            static_cast<int>(style.background->g),
                            static_cast<int>(style.background->b));
                }
                if (style.underlined) {
                    fprintf(file_, ESCAPE "[4m");
                }
                if (style.bold) {
                    fprintf(file_, ESCAPE "[1m");
                }
                fwrite(text.data(), 1, text.size(), file_);
                fprintf(file_, ESCAPE "[m");
            });
        } else {
            auto plain = style_text.Plain();
            fwrite(plain.data(), 1, plain.size(), file_);
        }
    }

  private:
    FILE* const file_;
    const StyledTextTheme* theme_ = &StyledTextTheme::kDefaultTheme;
};

}  // namespace

std::unique_ptr<StyledTextPrinter> StyledTextPrinter::Create(FILE* out) {
    return std::make_unique<PrinterPosix>(out);
}

StyledTextPrinter::~StyledTextPrinter() = default;

}  // namespace tint
