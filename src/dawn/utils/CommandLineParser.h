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

#ifndef SRC_DAWN_UTILS_COMMANDLINEPARSER_H_
#define SRC_DAWN_UTILS_COMMANDLINEPARSER_H_

#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "dawn/common/NonMovable.h"

namespace dawn::utils {

// A helper class to parse command line arguments.
//
//   CommandLineParser parser;
//   auto& dryRun = parser.AddBool().Name("dry-run").ShortName('d');
//   auto& input = parser.AddString().Name("input").ShortName('i');
//
//   parser.Parse(argc, argv);
//   if (dryRun.GetValue() && input.IsSet()) {
//     doStuffWith(input.GetValue());
//   }
//
// Command line options can use short-form for boolean options "(-f") and use both spaces or = to
// separate the value for an option ("-f=foo" and  "-f foo").

// TODO(42241992): Considering supporting more types of options and command line parsing niceties.
// - Support "-" with a bunch of short names (like grep -rniI)
// - Support "--" being used to separate remaining args.
// - Support showing a help.

class CommandLineParser {
  public:
    // The base class for all options to let them interact with the parser.
    class OptionBase : NonMovable {
      public:
        virtual ~OptionBase();

        const std::string& GetName() const;
        std::string GetShortName() const;
        const std::string& GetDescription() const;
        // Returns whether parser saw that option in the command line.
        bool IsSet() const;

        struct ParseResult {
            bool success;
            std::span<std::string_view> remainingArgs = {};
            std::string errorMessage = {};
        };
        ParseResult Parse(std::span<std::string_view> args);

      protected:
        virtual ParseResult ParseImpl(std::span<std::string_view> args) = 0;

        bool mSet = false;
        std::string mName;
        std::string mShortName;
        std::string mDescription;
    };

    // A CRTP wrapper around OptionBase to support the fluent setters better.
    template <typename Child>
    class Option : public OptionBase {
      public:
        Child& Name(std::string name);
        Child& ShortName(char shortName);
        Child& Description(std::string description);
    };

    // An option returning a bool.
    // Can be set multiple times on the command line if not using the explicit true/false version.
    class BoolOption : public Option<BoolOption> {
      public:
        ~BoolOption() override;
        bool GetValue() const;

      private:
        ParseResult ParseImpl(std::span<std::string_view> args) override;
        bool mValue = false;
    };
    BoolOption& AddBool();

    // An option returning a string.
    class StringOption : public Option<StringOption> {
      public:
        ~StringOption() override;
        std::string GetValue() const;

      private:
        ParseResult ParseImpl(std::span<std::string_view> args) override;
        std::string mValue;
    };
    StringOption& AddString();

    // An option returning a list of string split from a comma-separated argument, or the argument
    // being set multiple times (or both).
    class StringListOption : public Option<StringListOption> {
      public:
        ~StringListOption() override;
        std::span<const std::string> GetValue() const;

      private:
        ParseResult ParseImpl(std::span<std::string_view> args) override;
        std::vector<std::string> mValue;
    };
    StringListOption& AddStringList();

    // An option converting a string name to a value.
    //
    //   parser.AddEnum({{{"a", E::A}, {"b", E::B}}});
    template <typename E>
    class EnumOption : public Option<EnumOption<E>> {
      public:
        EnumOption(std::vector<std::pair<std::string_view, E>> conversions);
        ~EnumOption() override;
        E GetValue() const;

      private:
        OptionBase::ParseResult ParseImpl(std::span<std::string_view> args) override;
        E mValue;
        std::vector<std::pair<std::string_view, E>> mConversions;
    };
    template <typename E>
    EnumOption<E>& AddEnum(std::vector<std::pair<std::string_view, E>> conversions) {
        return AddOption(std::make_unique<EnumOption<E>>(std::move(conversions)));
    }

    // Helper structs for the Parse calls.
    struct ParseResult {
        bool success;
        std::string errorMessage = {};
    };
    struct ParseOptions {
        bool unknownIsError = true;
    };

    // Parse the arguments provided and set the options.
    ParseResult Parse(std::span<std::string_view> args,
                      const ParseOptions& parseOptions = {.unknownIsError = true});

    // Small wrappers around the previous Parse for ease of use.
    ParseResult Parse(const std::vector<std::string>& args,
                      const ParseOptions& parseOptions = {.unknownIsError = true});
    ParseResult Parse(int argc,
                      const char** argv,
                      const ParseOptions& parseOptions = {.unknownIsError = true});

  private:
    template <typename T>
    T& AddOption(std::unique_ptr<T> option) {
        T& result = *option.get();
        mOptions.push_back(std::move(option));
        return result;
    }

    std::vector<std::unique_ptr<OptionBase>> mOptions;
};

// Option<Child>
template <typename Child>
Child& CommandLineParser::Option<Child>::Name(std::string name) {
    mName = name;
    return static_cast<Child&>(*this);
}

template <typename Child>
Child& CommandLineParser::Option<Child>::ShortName(char shortName) {
    mShortName = shortName;
    return static_cast<Child&>(*this);
}

template <typename Child>
Child& CommandLineParser::Option<Child>::Description(std::string description) {
    mDescription = description;
    return static_cast<Child&>(*this);
}

// EnumOption<E>
template <typename E>
CommandLineParser::EnumOption<E>::EnumOption(
    std::vector<std::pair<std::string_view, E>> conversions)
    : mConversions(conversions) {}

template <typename E>
CommandLineParser::EnumOption<E>::~EnumOption<E>() = default;

template <typename E>
E CommandLineParser::EnumOption<E>::GetValue() const {
    return mValue;
}

template <typename E>
CommandLineParser::OptionBase::ParseResult CommandLineParser::EnumOption<E>::ParseImpl(
    std::span<std::string_view> args) {
    if (this->IsSet()) {
        return {false, args, "cannot be set multiple times"};
    }

    if (args.empty()) {
        return {false, args, "expected a value"};
    }

    for (auto conversion : mConversions) {
        if (conversion.first == args.front()) {
            mValue = conversion.second;
            return {true, args.subspan(1)};
        }
    }

    return {false, args, absl::StrFormat("unknown value \"%s\"", args.front())};
}

}  // namespace dawn::utils

#endif  // SRC_DAWN_UTILS_COMMANDLINEPARSER_H_
