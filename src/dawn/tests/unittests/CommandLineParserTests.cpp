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

#include "absl/strings/str_split.h"
#include "dawn/utils/CommandLineParser.h"
#include "gtest/gtest.h"

namespace dawn {
namespace {

using CLP = utils::CommandLineParser;

std::vector<std::string> Split(const char* s) {
    return absl::StrSplit(s, " ");
}

void ExpectSuccess(const CLP::ParseResult& result) {
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.errorMessage, "");
}

void ExpectError(const CLP::ParseResult& result, std::string_view message) {
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorMessage, message);
}

// TODO test passing twice for each of them.

// Tests for BoolOption parsing
TEST(CommandLineParserTest, BoolParsing) {
    // Test parsing with nothing afterwards.
    {
        CLP opts;
        auto& opt = opts.AddBool().Name("foo").ShortName('f');
        ExpectSuccess(opts.Parse({Split("-f")}));

        EXPECT_TRUE(opt.GetValue());
    }

    // Test parsing with another flag afterwards.
    {
        CLP opts;
        auto& opt = opts.AddBool().Name("foo").ShortName('f');
        auto& optB = opts.AddBool().Name("bar").ShortName('b');
        ExpectSuccess(opts.Parse(Split("-f -b")));

        EXPECT_TRUE(opt.GetValue());
        EXPECT_TRUE(optB.IsDefined());
    }

    // Test parsing with garbage afterwards.
    {
        CLP opts;
        auto& opt = opts.AddBool().Name("foo").ShortName('f');
        ExpectSuccess(opts.Parse(Split("-f garbage"), {.unknownIsError = false}));

        EXPECT_TRUE(opt.GetValue());
    }

    // Test parsing "true"
    {
        CLP opts;
        auto& opt = opts.AddBool().Name("foo").ShortName('f');
        auto& optB = opts.AddBool().Name("bar").ShortName('b');
        ExpectSuccess(opts.Parse(Split("-f true -b")));

        EXPECT_TRUE(opt.GetValue());
        EXPECT_TRUE(optB.IsDefined());
    }

    // Test parsing "false"
    {
        CLP opts;
        auto& opt = opts.AddBool().Name("foo").ShortName('f');
        auto& optB = opts.AddBool().Name("bar").ShortName('b');
        ExpectSuccess(opts.Parse(Split("-f false -b")));

        EXPECT_FALSE(opt.GetValue());
        EXPECT_TRUE(optB.IsDefined());
    }
}

// Tests for StringOption parsing.
TEST(CommandLineParserTest, StringParsing) {
    // Test with nothing afterwards
    {
        CLP opts;
        opts.AddString().Name("foo").ShortName('f');
        ExpectError(opts.Parse({Split("-f")}), "Failure while parsing \"foo\": expected a value");
    }

    // Test parsing with another flag afterwards.
    {
        CLP opts;
        auto& opt = opts.AddString().Name("foo").ShortName('f');
        auto& optB = opts.AddBool().Name("bar").ShortName('b');
        ExpectSuccess(opts.Parse(Split("-f -b")));

        EXPECT_EQ(opt.GetValue(), "-b");
        EXPECT_FALSE(optB.IsDefined());
    }

    // Test parsing with some data afterwards
    {
        CLP opts;
        auto& opt = opts.AddString().Name("foo").ShortName('f');
        ExpectSuccess(opts.Parse(Split("-f supercalifragilisticexpialidocious")));

        EXPECT_EQ(opt.GetValue(), "supercalifragilisticexpialidocious");
    }
}

// Tests for StringListOption parsing.
TEST(CommandLineParserTest, StringListParsing) {
    // Test with nothing afterwards
    {
        CLP opts;
        opts.AddStringList().Name("foo").ShortName('f');
        ExpectError(opts.Parse({Split("-f")}), "Failure while parsing \"foo\": expected a value");
    }

    // Test parsing with another flag afterwards.
    {
        CLP opts;
        auto& opt = opts.AddStringList().Name("foo").ShortName('f');
        auto& optB = opts.AddBool().Name("bar").ShortName('b');
        ExpectSuccess(opts.Parse(Split("-f -b")));

        EXPECT_EQ(opt.GetValue().size(), 1u);
        EXPECT_EQ(opt.GetValue()[0], "-b");
        EXPECT_FALSE(optB.IsDefined());
    }

    // Test parsing with some data afterwards
    {
        CLP opts;
        auto& opt = opts.AddStringList().Name("foo").ShortName('f');
        ExpectSuccess(opts.Parse(Split("-f sugar,butter,flour")));

        EXPECT_EQ(opt.GetValue().size(), 3u);
        EXPECT_EQ(opt.GetValue()[0], "sugar");
        EXPECT_EQ(opt.GetValue()[1], "butter");
        EXPECT_EQ(opt.GetValue()[2], "flour");
    }
}

// Tests for EnumOption parsing.
enum class Cell {
    Pop,
    Six,
    Squish,
    Uhuh,
    Cicero,
    Lipschitz,
};
TEST(CommandLineParserTest, EnumParsing) {
    std::vector<std::pair<std::string_view, Cell>> conversions = {{
        {"pop", Cell::Pop}, {"six", Cell::Six}, {"uh-uh", Cell::Uhuh},
        // others left as an exercise to the reader.
    }};

    // Test with nothing afterwards
    {
        CLP opts;
        opts.AddEnum<Cell>(conversions).Name("foo").ShortName('f');
        ExpectError(opts.Parse({Split("-f")}), "Failure while parsing \"foo\": expected a value");
    }

    // Test parsing with another flag afterwards.
    {
        CLP opts;
        opts.AddEnum<Cell>(conversions).Name("foo").ShortName('f');
        opts.AddBool().Name("bar").ShortName('b');
        ExpectError(opts.Parse({Split("-f -b")}),
                    "Failure while parsing \"foo\": unknown value \"-b\"");
    }

    // Test parsing a correct enum value.
    {
        CLP opts;
        auto& opt = opts.AddEnum<Cell>(conversions).Name("foo").ShortName('f');
        auto& optB = opts.AddBool().Name("bar").ShortName('b');
        ExpectSuccess(opts.Parse({Split("-f six -b")}));

        EXPECT_EQ(opt.GetValue(), Cell::Six);
        EXPECT_TRUE(optB.IsDefined());
    }
}

// TODO
// Long and short options.
// What if there is only a long name.
// Test skipping of non-flags with / without error.
// Unknown flag.

}  // anonymous namespace
}  // namespace dawn
