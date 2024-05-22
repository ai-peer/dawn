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

#include "src/tint/cmd/fuzz/ir/fuzz.h"

#if TINT_BUILD_IR_BINARY

#include "src/tint/utils/cli/cli.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

TINT_BEGIN_DISABLE_PROTOBUF_WARNINGS();
#include "src/tint/lang/core/ir/binary/ir.pb.h"
TINT_END_DISABLE_PROTOBUF_WARNINGS();

namespace {

tint::fuzz::ir::Options options;

DEFINE_BINARY_PROTO_FUZZER(const tint::core::ir::binary::pb::Module& pb) {
    tint::fuzz::ir::Run(pb, options);
}

}  // namespace

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
    tint::cli::OptionSet opts;

    tint::Vector<std::string_view, 8> arguments;
    for (int i = 1; i < *argc; i++) {
        std::string_view arg((*argv)[i]);
        if (!arg.empty()) {
            arguments.Push(arg);
        }
    }

    auto show_help = [&] {
        std::cerr << "Custom fuzzer options:" << '\n';
        opts.ShowHelp(std::cerr);
        std::cerr << '\n';
        // Change args to show libfuzzer help
        std::cerr << "Standard libfuzzer ";  // libfuzzer will print 'Usage:'
        static char help[] = "-help=1";
        *argc = 2;
        (*argv)[1] = help;
    };

    auto& opt_help = opts.Add<tint::cli::BoolOption>("help", "shows the usage");
    auto& opt_filter = opts.Add<tint::cli::StringOption>(
        "filter", "runs only the fuzzers with the given substring");
    auto& opt_concurrent =
        opts.Add<tint::cli::BoolOption>("concurrent", "runs the fuzzers concurrently");
    auto& opt_verbose =
        opts.Add<tint::cli::BoolOption>("verbose", "prints the name of each fuzzer before running");

    tint::cli::ParseOptions parse_opts;
    parse_opts.ignore_unknown = true;
    if (auto res = opts.Parse(arguments, parse_opts); res != tint::Success) {
        show_help();
        std::cerr << res.Failure();
        return 0;
    }

    if (opt_help.value.value_or(false)) {
        show_help();
        return 0;
    }

    options.filter = opt_filter.value.value_or("");
    options.run_concurrently = opt_concurrent.value.value_or(false);
    options.verbose = opt_verbose.value.value_or(false);
    return 0;
}

#endif
