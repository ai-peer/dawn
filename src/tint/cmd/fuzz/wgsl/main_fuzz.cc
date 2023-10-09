// Copyright 2023 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/tint/cmd/fuzz/wgsl/wgsl_fuzz.h"

#include <iostream>

#include "src/tint/utils/cli/cli.h"

namespace {

tint::fuzz::wgsl::Options options;

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 0) {
        std::string_view wgsl(reinterpret_cast<const char*>(data), size);
        tint::fuzz::wgsl::Run(wgsl, options);
    }
    return 0;
}

extern "C" int LLVMFuzzerInitialize(int* argc, const char*** argv) {
    tint::cli::OptionSet opts;

    tint::Vector<std::string_view, 8> arguments;
    for (int i = 1; i < *argc; i++) {
        std::string_view arg((*argv)[i]);
        if (!arg.empty()) {
            arguments.Push(arg);
        }
    }

    auto show_help = [&] {
        std::cerr << "Custom fuzzer options:" << std::endl;
        opts.ShowHelp(std::cerr);
        std::cerr << std::endl;
        // Change args to show libfuzzer help
        std::cerr << "Standard libfuzzer ";  // libfuzzer will print 'Usage:'
        static const char* help[] = {(*argv)[0], "-help=1"};
        *argc = 2;
        *argv = help;
    };

    auto& opt_help = opts.Add<tint::cli::BoolOption>("help", "shows the usage");
    auto& opt_concurrent =
        opts.Add<tint::cli::BoolOption>("concurrent", "runs the fuzzers concurrently");

    tint::cli::ParseOptions parse_opts;
    parse_opts.ignore_unknown = true;
    if (auto res = opts.Parse(arguments, parse_opts); !res) {
        show_help();
        std::cerr << res.Failure();
        return 0;
    }

    if (opt_help.value.value_or(false)) {
        show_help();
        return 0;
    }

    options.run_concurrently = opt_concurrent.value.value_or(false);
    return 0;
}
