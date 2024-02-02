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

#include "src/tint/utils/text/styled_text_printer.h"
#include "src/tint/utils/text/text_style.h"

#include "gtest/gtest.h"

namespace tint {
namespace {

// Actually verifying that the expected colors are printed is exceptionally
// difficult as:
// a) The color emission varies by OS.
// b) The logic checks to see if the printer is writing to a terminal, making mocking hard.
// c) Actually probing what gets written to a FILE* is notoriously tricky.
//
// The least we can do is to exercise the code - which is what we do here.
// The test will print each of the colors, and can be examined with human eyeballs.
// This can be enabled or disabled with ENABLE_PRINTER_TESTS
#define ENABLE_PRINTER_TESTS 0
#if ENABLE_PRINTER_TESTS

using StyledTextPrinterTest = testing::Test;

TEST_F(StyledTextPrinterTest, ForegroundColors) {
    auto printer = StyledTextPrinter::Create(stdout, true);

    TextStyle style;
    for (int b_base = 0; b_base < 256; b_base += 4 * 16) {
        for (int g = 0; g < 256; g += 32) {
            int b = b_base;
            for (int i = 0; i < 4; i++, b += 16) {
                for (int r = 0; r < 256; r += 16) {
                    style.foreground = Color{
                        static_cast<uint8_t>(r),
                        static_cast<uint8_t>(g),
                        static_cast<uint8_t>(b),
                    };
                    printer->Print("◼", style);
                }
                printer->Print("  ", TextStyle{});
            }
            printer->Print("\n", TextStyle{});
        }
        printer->Print("\n", TextStyle{});
    }
}

TEST_F(StyledTextPrinterTest, BackgroundColors) {
    auto printer = StyledTextPrinter::Create(stdout, true);

    TextStyle style;
    for (int b_base = 0; b_base < 256; b_base += 4 * 16) {
        for (int g = 0; g < 256; g += 32) {
            int b = b_base;
            for (int i = 0; i < 4; i++, b += 16) {
                for (int r = 0; r < 256; r += 16) {
                    style.background = Color{
                        static_cast<uint8_t>(255 - r),
                        static_cast<uint8_t>(255 - g),
                        static_cast<uint8_t>(255 - b),
                    };
                    printer->Print("◼", style);
                }
                printer->Print("  ", TextStyle{});
            }
            printer->Print("\n", TextStyle{});
        }
        printer->Print("\n", TextStyle{});
    }
}

TEST_F(StyledTextPrinterTest, BoldUnderlined) {
    auto printer = StyledTextPrinter::Create(stdout, true);

    printer->Print("Plain\n", TextStyle{});
    printer->Print("Bold\n", TextStyle{}.Bold());
    printer->Print("Underlined\n", TextStyle{}.Underline());
    printer->Print("Bold + Underlined\n", TextStyle{}.Bold().Underline());
}

#endif  // ENABLE_PRINTER_TESTS

}  // namespace
}  // namespace tint
