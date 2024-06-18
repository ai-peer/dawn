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
        EXPECT_TRUE(optB.IsSet());
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
        EXPECT_TRUE(optB.IsSet());
    }

    // Test parsing "false"
    {
        CLP opts;
        auto& opt = opts.AddBool().Name("foo").ShortName('f');
        auto& optB = opts.AddBool().Name("bar").ShortName('b');
        ExpectSuccess(opts.Parse(Split("-f false -b")));

        EXPECT_FALSE(opt.GetValue());
        EXPECT_TRUE(optB.IsSet());
    }

    // Test parsing the option multiple times, with an explicit true argument.
    {
        CLP opts;
        opts.AddBool().Name("foo").ShortName('f');
        ExpectError(opts.Parse({Split("-f --foo true")}),
                    "Failure while parsing \"foo\": cannot set multiple times with explicit "
                    "true/false arguments");
    }

    // Test parsing the option multiple times, with an explicit false argument.
    {
        CLP opts;
        opts.AddBool().Name("foo").ShortName('f');
        ExpectError(opts.Parse({Split("-f --foo false")}),
                    "Failure while parsing \"foo\": cannot set multiple times with explicit "
                    "true/false arguments");
    }

    // Test parsing the option multiple times, with the implicit true argument.
    {
        CLP opts;
        auto& opt = opts.AddBool().Name("foo").ShortName('f');
        ExpectSuccess(opts.Parse({Split("-f -f")}));

        EXPECT_TRUE(opt.GetValue());
    }

    // Test parsing the option multiple times, with the implicit true argument but conflicting
    // values
    {
        CLP opts;
        opts.AddBool().Name("foo").ShortName('f');
        ExpectError(opts.Parse({Split("-f false --foo")}),
                    "Failure while parsing \"foo\": cannot be set to both true and false");
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
        EXPECT_FALSE(optB.IsSet());
    }

    // Test parsing with some data afterwards
    {
        CLP opts;
        auto& opt = opts.AddString().Name("foo").ShortName('f');
        ExpectSuccess(opts.Parse(Split("-f supercalifragilisticexpialidocious")));

        EXPECT_EQ(opt.GetValue(), "supercalifragilisticexpialidocious");
    }

    // Test setting multiple times
    {
        CLP opts;
        opts.AddString().Name("foo").ShortName('f');
        ExpectError(opts.Parse({Split("-f aa -f aa")}),
                    "Failure while parsing \"foo\": cannot be set multiple times");
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
        EXPECT_FALSE(optB.IsSet());
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

    // Test passing the option mutliple times, it should add to the list.
    {
        CLP opts;
        auto& opt = opts.AddStringList().Name("foo").ShortName('f');
        ExpectSuccess(opts.Parse(Split("-f sugar -foo butter,flour")));

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
        EXPECT_TRUE(optB.IsSet());
    }

    // Test setting multiple times
    {
        CLP opts;
        opts.AddEnum<Cell>(conversions).Name("foo").ShortName('f');
        ExpectError(opts.Parse({Split("-f six -f six")}),
                    "Failure while parsing \"foo\": cannot be set multiple times");
    }
}

// Various tests for the handling of long and short names.
TEST(CommandLineParserTest, LongAndShortNames) {
    // An option can be referenced by both a long and short name.
    {
        CLP opts;
        auto& opt = opts.AddStringList().Name("foo").ShortName('f');
        ExpectSuccess(opts.Parse(Split("-f sugar -foo butter,flour")));

        EXPECT_EQ(opt.GetValue().size(), 3u);
        EXPECT_EQ(opt.GetValue()[0], "sugar");
        EXPECT_EQ(opt.GetValue()[1], "butter");
        EXPECT_EQ(opt.GetValue()[2], "flour");
    }

    // An option without a short name cannot be referenced with it.
    {
        CLP opts;
        opts.AddStringList().Name("foo");
        ExpectError(opts.Parse(Split("-f sugar -foo butter,flour")), "Unknown option \"f\"");
    }

    // An option without a long name cannot be referenced with it.
    {
        CLP opts;
        opts.AddStringList().ShortName('f');
        ExpectError(opts.Parse(Split("-f sugar -foo butter,flour")), "Unknown option \"foo\"");
    }

    // Regression test for two options having no short name.
    {
        CLP opts;
        opts.AddStringList().Name("foo");
        opts.AddStringList().Name("bar");
        ExpectSuccess(opts.Parse(0, nullptr));
    }
}

// Tests for option names not being recognized.
TEST(CommandLineParserTest, UnknownOption) {
    // An empty arg is not a known option.
    {
        CLP opts;
        ExpectError(opts.Parse({{""}}), "Unknown option \"\"");
        ExpectSuccess(opts.Parse({{""}}, {.unknownIsError = false}));
    }

    // A - is not a known option.
    {
        CLP opts;
        ExpectError(opts.Parse({{"-"}}), "Unknown option \"\"");
        ExpectSuccess(opts.Parse({{"-"}}, {.unknownIsError = false}));
    }

    // A -- is not a known option.
    {
        CLP opts;
        ExpectError(opts.Parse({{"--"}}), "Unknown option \"\"");
        ExpectSuccess(opts.Parse({{"--"}}, {.unknownIsError = false}));
    }

    // An unknown short name option.
    {
        CLP opts;
        ExpectError(opts.Parse({{"-f"}}), "Unknown option \"f\"");
        ExpectError(opts.Parse({{"-f="}}), "Unknown option \"f\"");
        ExpectSuccess(opts.Parse({{"-f"}}, {.unknownIsError = false}));
        ExpectSuccess(opts.Parse({{"-f="}}, {.unknownIsError = false}));
    }

    // An unknown long name option.
    {
        CLP opts;
        ExpectError(opts.Parse({{"-foo"}}), "Unknown option \"foo\"");
        ExpectError(opts.Parse({{"-foo="}}), "Unknown option \"foo\"");
        ExpectSuccess(opts.Parse({{"-foo"}}, {.unknownIsError = false}));
        ExpectSuccess(opts.Parse({{"-foo="}}, {.unknownIsError = false}));
    }
}

// Tests for options being set with =
TEST(CommandLineParserTest, EqualSeparator) {
    // Test that using an = separator works and lets other arguments be consumed.
    {
        CLP opts;
        auto& opt = opts.AddStringList().Name("foo").ShortName('f');
        ExpectSuccess(opts.Parse(Split("-f=sugar -foo butter,flour")));

        EXPECT_EQ(opt.GetValue().size(), 3u);
        EXPECT_EQ(opt.GetValue()[0], "sugar");
        EXPECT_EQ(opt.GetValue()[1], "butter");
        EXPECT_EQ(opt.GetValue()[2], "flour");
    }

    // Test that if the part after the = is not consumed there is an error.
    {
        CLP opts;
        opts.AddBool().Name("foo").ShortName('f');
        ExpectError(opts.Parse({Split("-f=garbage")}),
                    "Argument \"garbage\" was not valid for option \"foo\"");
    }
}

}  // anonymous namespace
}  // namespace dawn
