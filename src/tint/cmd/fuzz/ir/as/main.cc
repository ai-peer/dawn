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

#include <charconv>
#include <cstdio>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "src/tint/lang/wgsl/sem/variable.h"

#include "src/tint/api/tint.h"
#include "src/tint/cmd/common/helper.h"
#include "src/tint/lang/core/ir/disassembler.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/wgsl/ast/module.h"
#include "src/tint/lang/wgsl/ast/transform/first_index_offset.h"
#include "src/tint/lang/wgsl/ast/transform/manager.h"
#include "src/tint/lang/wgsl/ast/transform/renamer.h"
#include "src/tint/lang/wgsl/ast/transform/single_entry_point.h"
#include "src/tint/lang/wgsl/ast/transform/substitute_override.h"
#include "src/tint/lang/wgsl/common/validation_mode.h"
#include "src/tint/lang/wgsl/helpers/flatten_bindings.h"
#include "src/tint/utils/cli/cli.h"
#include "src/tint/utils/command/command.h"
#include "src/tint/utils/containers/transform.h"
#include "src/tint/utils/diagnostic/formatter.h"
#include "src/tint/utils/macros/defer.h"
#include "src/tint/utils/system/env.h"
#include "src/tint/utils/system/terminal.h"
#include "src/tint/utils/text/string.h"
#include "src/tint/utils/text/string_stream.h"
#include "src/tint/utils/text/styled_text.h"
#include "src/tint/utils/text/styled_text_printer.h"
#include "src/tint/utils/text/styled_text_theme.h"

#include "src/tint/lang/wgsl/reader/program_to_ir/program_to_ir.h"
#include "src/tint/lang/wgsl/reader/reader.h"

#include "src/tint/lang/core/ir/binary/encode.h"
#include "src/tint/lang/core/ir/validator.h"
#include "src/tint/lang/wgsl/helpers/apply_substitute_overrides.h"
#include "src/tint/utils/text/color_mode.h"

namespace {

struct Options {
    std::unique_ptr<tint::StyledTextPrinter> printer;

    std::string input_filename;
    std::string output_file = "-";  // Default to stdout

    tint::Vector<std::string, 4> transforms;
    tint::Hashmap<std::string, double, 8> overrides;

    std::string ep_name;

    bool verbose = false;
    bool dump_inspector_bindings = false;
    bool emit_single_entry_point = false;

    bool rename_all = false;

    bool dump_ir = false;
    bool dump_ir_bin = false;
};

bool ParseArgs(tint::VectorRef<std::string_view> arguments,
               std::string transform_names,
               Options* opts) {
    using namespace tint::cli;  // NOLINT(build/namespaces)

    OptionSet options;
    auto& col = options.Add<EnumOption<tint::ColorMode>>(
        "color", "Use colored output",
        tint::Vector{
            EnumName{tint::ColorMode::kPlain, "off"},
            EnumName{tint::ColorMode::kDark, "dark"},
            EnumName{tint::ColorMode::kLight, "light"},
        },
        ShortName{"col"}, Default{tint::ColorModeDefault()});
    TINT_DEFER(opts->printer = CreatePrinter(*col.value));

    auto& ep = options.Add<StringOption>("entry-point", "Output single entry point",
                                         ShortName{"ep"}, Parameter{"name"});
    TINT_DEFER({
        if (ep.value.has_value()) {
            opts->ep_name = *ep.value;
            opts->emit_single_entry_point = true;
        }
    });

    auto& output = options.Add<StringOption>("output-name", "Output file name", ShortName{"o"},
                                             Parameter{"name"});
    TINT_DEFER(opts->output_file = output.value.value_or(""));

    auto& dump_ir = options.Add<BoolOption>("dump-ir", "Writes the IR to stdout", Alias{"emit-ir"},
                                            Default{false});
    TINT_DEFER(opts->dump_ir = *dump_ir.value);

    auto& dump_ir_bin = options.Add<BoolOption>(
        "dump-ir-bin", "Writes the IR as a human readable protobuf to stdout", Alias{"emit-ir-bin"},
        Default{false});
    TINT_DEFER(opts->dump_ir_bin = *dump_ir_bin.value);

    auto& verbose =
        options.Add<BoolOption>("verbose", "Verbose output", ShortName{"v"}, Default{false});
    TINT_DEFER(opts->verbose = *verbose.value);

    auto& rename_all = options.Add<BoolOption>("rename-all", "Renames all symbols", Default{false});
    TINT_DEFER(opts->rename_all = *rename_all.value);

    auto& dump_inspector_bindings = options.Add<BoolOption>(
        "dump-inspector-bindings", "Dump reflection data about bindings to stdout",
        Alias{"emit-inspector-bindings"}, Default{false});
    TINT_DEFER(opts->dump_inspector_bindings = *dump_inspector_bindings.value);

    auto& transforms =
        options.Add<StringOption>("transform", R"(Runs transforms, name list is comma separated
Available transforms:
)" + transform_names,
                                  ShortName{"t"});
    TINT_DEFER({
        if (transforms.value.has_value()) {
            for (auto transform : tint::Split(*transforms.value, ",")) {
                opts->transforms.Push(std::string(transform));
            }
        }
    });

    auto& overrides = options.Add<StringOption>(
        "overrides", "Override values as IDENTIFIER=VALUE, comma-separated");

    auto& help = options.Add<BoolOption>("help", "Show usage", ShortName{"h"});

    auto show_usage = [&] {
        std::cout << R"(Usage: tint [options] <input-file>

Options:
)";
        options.ShowHelp(std::cout);
    };

    auto result = options.Parse(arguments);
    if (result != tint::Success) {
        std::cerr << result.Failure() << "\n";
        show_usage();
        return false;
    }
    if (help.value.value_or(false)) {
        show_usage();
        return false;
    }

    if (overrides.value.has_value()) {
        for (const auto& o : tint::Split(*overrides.value, ",")) {
            auto parts = tint::Split(o, "=");
            if (parts.Length() != 2) {
                std::cerr << "override values must be of the form IDENTIFIER=VALUE";
                return false;
            }
            auto value = tint::strconv::ParseNumber<double>(parts[1]);
            if (value != tint::Success) {
                std::cerr << "invalid override value: " << parts[1];
                return false;
            }
            opts->overrides.Add(std::string(parts[0]), value.Get());
        }
    }

    auto files = result.Get();
    if (files.Length() > 1) {
        std::cerr << "More than one input file specified: "
                  << tint::Join(Transform(files, tint::Quote), ", ") << "\n";
        return false;
    }
    if (files.Length() == 1) {
        opts->input_filename = files[0];
    }

    return true;
}

/// Writes the given `buffer` into the file named as `output_file` using the
/// given `mode`.  If `output_file` is empty or "-", writes to standard
/// output. If any error occurs, returns false and outputs error message to
/// standard error. The ContainerT type must have data() and size() methods,
/// like `std::string` and `std::vector` do.
/// @returns true on success
template <typename ContainerT>
[[maybe_unused]] bool WriteFile(const std::string& output_file,
                                const std::string mode,
                                const ContainerT& buffer) {
    const bool use_stdout = output_file.empty() || output_file == "-";
    FILE* file = stdout;

    if (!use_stdout) {
#if defined(_MSC_VER)
        fopen_s(&file, output_file.c_str(), mode.c_str());
#else
        file = fopen(output_file.c_str(), mode.c_str());
#endif
        if (!file) {
            std::cerr << "Could not open file " << output_file << " for writing\n";
            return false;
        }
    }

    size_t written =
        fwrite(buffer.data(), sizeof(typename ContainerT::value_type), buffer.size(), file);
    if (buffer.size() != written) {
        if (use_stdout) {
            std::cerr << "Could not write all output to standard output\n";
        } else {
            std::cerr << "Could not write to file " << output_file << "\n";
            fclose(file);
        }
        return false;
    }
    if (!use_stdout) {
        fclose(file);
    }

    return true;
}

/// Generate IR code for a program.
/// @param program the program to generate
/// @param options the options that Tint was invoked with
/// @returns true on success
bool GenerateIr([[maybe_unused]] const tint::Program& program,
                [[maybe_unused]] const Options& options) {
    auto result = tint::wgsl::reader::ProgramToLoweredIR(program);
    if (result != tint::Success) {
        std::cerr << "Failed to build IR from program: " << result.Failure() << "\n";
        return false;
    }

    options.printer->Print(tint::core::ir::Disassembler(result.Get()).Text());
    options.printer->Print(tint::StyledText{} << "\n");

    return true;
}

/// Generate an IR module for a program, performs checking for unsupported
/// enables, needed transforms, and validation.
/// @param program the program to generate
/// @param options the options that Tint was invoked with
/// @returns generated module on success, tint::failure on failure
tint::Result<tint::core::ir::Module> GenerateIrModule([[maybe_unused]] const tint::Program& program,
                                                      [[maybe_unused]] const Options& options) {
    if (program.AST().Enables().Any(tint::wgsl::reader::IsUnsupportedByIR)) {
        return tint::Failure{"Unsupported enable used in shader"};
    }

    auto transformed = tint::wgsl::ApplySubstituteOverrides(program);
    auto& src = transformed ? transformed.value() : program;
    if (!src.IsValid()) {
        return tint::Failure{src.Diagnostics()};
    }

    auto ir = tint::wgsl::reader::ProgramToLoweredIR(src);
    if (ir != tint::Success) {
        return ir.Failure();
    }

    if (auto val = tint::core::ir::Validate(ir.Get()); val != tint::Success) {
        return val.Failure();
    }

    return ir;
}

/// Generate IR binary protobuf for a program.
/// @param program the program to generate
/// @param options the options that Tint was invoked with
/// @returns true on success
bool GenerateIrProtoBinary([[maybe_unused]] const tint::Program& program,
                           [[maybe_unused]] const Options& options) {
    auto module = GenerateIrModule(program, options);
    if (module != tint::Success) {
        std::cerr << "Failed to generate lowered IR from program: " << module.Failure() << "\n";
        return false;
    }

    auto pb = tint::core::ir::binary::Encode(module.Get());
    if (pb != tint::Success) {
        std::cerr << "Failed to encode IR module to protobuf: " << pb.Failure() << "\n";
        return false;
    }

    if (!WriteFile(options.output_file, "wb", ToStdVector(pb.Get()))) {
        std::cerr << "Failed to write protobuf binary out to file\n";
        return false;
    }

    return true;
}

/// Generate IR human readable protobuf for a program.
/// @param program the program to generate
/// @param options the options that Tint was invoked with
/// @returns true on success
bool GenerateIrProtoDebug([[maybe_unused]] const tint::Program& program,
                          [[maybe_unused]] const Options& options) {
    auto module = GenerateIrModule(program, options);
    if (module != tint::Success) {
        std::cerr << "Failed to generate lowered IR from program: " << module.Failure() << "\n";
        return false;
    }

    auto pb = tint::core::ir::binary::EncodeDebug(module.Get());
    if (pb != tint::Success) {
        std::cerr << "Failed to encode IR module to protobuf: " << pb.Failure() << "\n";
        return false;
    }

    if (!WriteFile(options.output_file, "w", pb.Get())) {
        std::cerr << "Failed to write protobuf debug text out to file\n";
        return false;
    }

    return true;
}

}  // namespace

int main(int argc, const char** argv) {
    tint::Vector<std::string_view, 8> arguments;
    for (int i = 1; i < argc; i++) {
        std::string_view arg(argv[i]);
        if (!arg.empty()) {
            arguments.Push(argv[i]);
        }
    }

    Options options;

    tint::Initialize();
    tint::SetInternalCompilerErrorReporter(&tint::cmd::TintInternalCompilerErrorReporter);

    struct TransformFactory {
        const char* name;
        /// Build and adds the transform to the transform manager.
        /// Parameters:
        ///   inspector - an inspector created from the parsed program
        ///   manager   - the transform manager. Add transforms to this.
        ///   inputs    - the input data to the transform manager. Add inputs to this.
        /// Returns true on success, false on error (program will immediately exit)
        std::function<bool(tint::inspector::Inspector& inspector,
                           tint::ast::transform::Manager& manager,
                           tint::ast::transform::DataMap& inputs)>
            make;
    };
    std::vector<TransformFactory> transforms = {
        {"first_index_offset",
         [](tint::inspector::Inspector&, tint::ast::transform::Manager& m,
            tint::ast::transform::DataMap& i) {
             i.Add<tint::ast::transform::FirstIndexOffset::BindingPoint>(0, 0);
             m.Add<tint::ast::transform::FirstIndexOffset>();
             return true;
         }},
        {"renamer",
         [](tint::inspector::Inspector&, tint::ast::transform::Manager& m,
            tint::ast::transform::DataMap&) {
             m.Add<tint::ast::transform::Renamer>();
             return true;
         }},
        {"robustness",
         [&](tint::inspector::Inspector&, tint::ast::transform::Manager&,
             tint::ast::transform::DataMap&) {  // enabled via writer option
             return true;
         }},
        {"substitute_override",
         [&](tint::inspector::Inspector& inspector, tint::ast::transform::Manager& m,
             tint::ast::transform::DataMap& i) {
             tint::ast::transform::SubstituteOverride::Config cfg;

             std::unordered_map<tint::OverrideId, double> values;
             values.reserve(options.overrides.Count());

             for (auto& override : options.overrides) {
                 const auto& name = override.key.Value();
                 const auto& value = override.value;
                 if (name.empty()) {
                     std::cerr << "empty override name\n";
                     return false;
                 }
                 if (auto num = tint::strconv::ParseNumber<decltype(tint::OverrideId::value)>(name);
                     num == tint::Success) {
                     tint::OverrideId id{num.Get()};
                     values.emplace(id, value);
                 } else {
                     auto override_names = inspector.GetNamedOverrideIds();
                     auto it = override_names.find(name);
                     if (it == override_names.end()) {
                         std::cerr << "unknown override '" << name << "'\n";
                         return false;
                     }
                     values.emplace(it->second, value);
                 }
             }

             cfg.map = std::move(values);

             i.Add<tint::ast::transform::SubstituteOverride::Config>(cfg);
             m.Add<tint::ast::transform::SubstituteOverride>();
             return true;
         }},
    };
    auto transform_names = [&] {
        tint::StringStream names;
        for (auto& t : transforms) {
            names << "   " << t.name << "\n";
        }
        return names.str();
    };

    if (!ParseArgs(arguments, transform_names(), &options)) {
        return EXIT_FAILURE;
    }

    tint::cmd::LoadProgramOptions opts;
    opts.filename = options.input_filename;
    opts.printer = options.printer.get();

    auto info = tint::cmd::LoadProgramInfo(opts);

    if (options.dump_ir) {
        GenerateIr(info.program, options);
    }

    if (options.dump_ir_bin) {
        GenerateIrProtoDebug(info.program, options);
    }

    tint::inspector::Inspector inspector(info.program);
    if (options.dump_inspector_bindings) {
        tint::cmd::PrintInspectorBindings(inspector);
    }

    tint::ast::transform::Manager transform_manager;
    tint::ast::transform::DataMap transform_inputs;

    // Renaming must always come first
    if (options.rename_all) {
        transform_manager.Add<tint::ast::transform::Renamer>();
    }

    auto enable_transform = [&](std::string_view name) {
        for (auto& t : transforms) {
            if (t.name == name) {
                return t.make(inspector, transform_manager, transform_inputs);
            }
        }

        std::cerr << "Unknown transform: " << name << "\n";
        std::cerr << "Available transforms: \n" << transform_names() << "\n";
        return false;
    };

    // If overrides are provided, add the SubstituteOverride transform.
    if (!options.overrides.IsEmpty()) {
        if (!enable_transform("substitute_override")) {
            return EXIT_FAILURE;
        }
    }

    for (const auto& name : options.transforms) {
        // TODO(dsinclair): The vertex pulling transform requires setup code to
        // be run that needs user input. Should we find a way to support that here
        // maybe through a provided file?
        if (!enable_transform(name)) {
            return EXIT_FAILURE;
        }
    }

    if (options.emit_single_entry_point) {
        transform_manager.append(std::make_unique<tint::ast::transform::SingleEntryPoint>());
        transform_inputs.Add<tint::ast::transform::SingleEntryPoint::Config>(options.ep_name);
    }

    tint::ast::transform::DataMap outputs;
    auto program = transform_manager.Run(info.program, std::move(transform_inputs), outputs);
    if (!program.IsValid()) {
        tint::cmd::PrintWGSL(std::cerr, program);
        std::cerr << program.Diagnostics() << "\n";
        return EXIT_FAILURE;
    }

    if (!GenerateIrProtoBinary(program, options)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
