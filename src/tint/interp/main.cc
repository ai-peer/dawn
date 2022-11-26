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

#include <iostream>
#include <vector>

#include "data_race_detector.h"
#include "interactive_debugger.h"
#include "shader_executor.h"
#include "tint/tint.h"

namespace {

const char kUsage[] = R"(Usage: tint-interp [options] <source-file> <entry-point>

options:
      --drd             Enable data race detection
  -h, --help            This help text
  -i, --interactive     Enable interactive mode
)";

struct Options {
    bool data_race_detector = false;
    bool show_help = false;
    bool interactive = false;

    std::string filename;
    std::string entry_point;
};

bool ParseArgs(const std::vector<std::string>& args, Options* opts) {
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--drd") {
            opts->data_race_detector = true;
        } else if (arg == "-h" || arg == "--help") {
            opts->show_help = true;
        } else if (arg == "-i" || arg == "--interactive") {
            opts->interactive = true;
        } else if (!arg.empty()) {
            if (arg[0] == '-') {
                std::cerr << "Unrecognized option: " << arg << std::endl;
                return false;
            }
            if (opts->filename.empty()) {
                opts->filename = arg;
            } else if (opts->entry_point.empty()) {
                opts->entry_point = arg;
            } else {
                std::cerr << "Too many positional arguments specified" << std::endl;
                return false;
            }
        }
    }
    return true;
}

bool LoadFile(std::string input_file, std::string* contents) {
    if (!contents) {
        std::cerr << "The contents pointer was null" << std::endl;
        return false;
    }

    FILE* file = nullptr;
#if defined(_MSC_VER)
    fopen_s(&file, input_file.c_str(), "rb");
#else
    file = fopen(input_file.c_str(), "rb");
#endif
    if (!file) {
        std::cerr << "Failed to open " << input_file << std::endl;
        return false;
    }

    fseek(file, 0, SEEK_END);
    const size_t file_size = static_cast<size_t>(ftell(file));
    fseek(file, 0, SEEK_SET);

    contents->clear();
    contents->resize(file_size);

    size_t bytes_read = fread(contents->data(), 1, file_size, file);
    fclose(file);
    if (bytes_read != file_size) {
        std::cerr << "Failed to read " << input_file << std::endl;
        return false;
    }

    return true;
}

[[noreturn]] void TintInternalCompilerErrorReporter(const tint::diag::List& diagnostics) {
    auto printer = tint::diag::Printer::create(stderr, true);
    tint::diag::Formatter{}.format(diagnostics, printer.get());
    tint::diag::Style bold_red{tint::diag::Color::kRed, true};
    constexpr const char* please_file_bug = R"(
********************************************************************
*  The tint shader compiler has encountered an unexpected error.   *
*                                                                  *
*  Please help us fix this issue by submitting a bug report at     *
*  crbug.com/tint with the source program that triggered the bug.  *
********************************************************************
)";
    printer->write(please_file_bug, bold_red);
    exit(1);
}

}  // namespace

int main(int argc, const char** argv) {
    Options options;

    // Parse command-line options.
    std::vector<std::string> args(argv, argv + argc);
    if (!ParseArgs(args, &options)) {
        std::cerr << "Failed to parse arguments." << std::endl;
        return 1;
    }
    if (options.show_help) {
        std::cout << kUsage << std::endl;
        return 0;
    }
    if (options.filename.empty()) {
        std::cerr << "Missing input filename" << std::endl;
        return 1;
    }
    if (options.entry_point.empty()) {
        std::cerr << "Missing entry point name" << std::endl;
        return 1;
    }

    // Load contents of source file.
    std::string source;
    if (!LoadFile(options.filename, &source)) {
        return 1;
    }

    // Initialize Tint.
    tint::Initialize();
    tint::SetInternalCompilerErrorReporter(&TintInternalCompilerErrorReporter);

    // Parse the source file to produce a Tint program object.
    auto source_file = std::make_unique<tint::Source::File>(options.filename, source);
    auto program = std::make_unique<tint::Program>(tint::reader::wgsl::Parse(source_file.get()));
    auto diag_printer = tint::diag::Printer::create(stderr, true);
    tint::diag::Formatter diag_formatter;
    if (!program) {
        std::cerr << "Failed to parse input file: " << options.filename << std::endl;
        return 1;
    }
    if (program->Diagnostics().count() > 0) {
        diag_formatter.format(program->Diagnostics(), diag_printer.get());
    }
    if (!program->IsValid()) {
        return 1;
    }

    // Create the shader executor.
    // TODO: Generate named override values.
    std::unique_ptr<tint::interp::ShaderExecutor> executor;
    {
        auto result = tint::interp::ShaderExecutor::Create(*program, options.entry_point, {});
        if (!result) {
            std::cerr << "Create failed: " << result.Failure() << std::endl;
            return 1;
        } else {
            executor = result.Move();
        }
    }

    // Enable data race detection if requested.
    std::unique_ptr<tint::interp::DataRaceDetector> data_race_detector;
    if (options.data_race_detector) {
        data_race_detector = std::make_unique<tint::interp::DataRaceDetector>(*executor);
    }

    // Enable interactive mode if requested.
    std::unique_ptr<tint::interp::InteractiveDebugger> debugger;
    if (options.interactive) {
        debugger = std::make_unique<tint::interp::InteractiveDebugger>(*executor);
    }

    // Run the executor.
    // TODO: Generate resource bindings.
    auto result = executor->Run({1, 1, 1}, {});
    if (!result) {
        std::cerr << "Run failed: " << result.Failure() << std::endl;
        return 1;
    }

    return 0;
}
