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

#include "tint/interp/interactive_debugger.h"

#include <charconv>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#define isatty _isatty
#define STDOUT_FILENO _fileno(stdout)
#else
#include <unistd.h>
#endif

#ifdef TINT_DEBUGGER_ENABLE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

#include "tint/interp/invocation.h"
#include "tint/interp/shader_executor.h"
#include "tint/interp/workgroup.h"
#include "tint/lang/wgsl/ast/expression.h"
#include "tint/lang/wgsl/ast/statement.h"
#include "tint/utils/text/styled_text_printer.h"

namespace tint::interp {

namespace {
// Helper to output an ANSI escape code and then reset it at the end of the current scope.
class ScopedRichText {
  public:
    ScopedRichText(bool use_rich_text, const char* code) : enabled(use_rich_text) {
        if (enabled) {
            std::cout << code;
        }
    }
    ~ScopedRichText() {
        if (enabled) {
            std::cout << "\u001b[0m";
        }
    }

  private:
    bool enabled;
};
#define SCOPED_RICH_TEXT(code) \
    ScopedRichText TINT_CONCAT(scoped_rich_text, __LINE__)(use_rich_text_, code)
}  // namespace

InteractiveDebugger::InteractiveDebugger(ShaderExecutor& executor, std::istream& input)
    : source_(executor.SourceFile()), executor_(executor), input_(input) {
    diag_printer_ = StyledTextPrinter::Create(stderr);
    interactive_ = isatty(STDOUT_FILENO);
#if defined(_WIN32)
    // TODO(crbug.com/1857): Enable colorized output on Windows.
    use_rich_text_ = false;
#else
    use_rich_text_ = interactive_;
#endif

    // Build a map of possible breakpoints.
    // Each line has at most one possible breakpoint, which is the first non-identifier expression
    // on the line that must be evaluated at runtime, or the first non-block statement on the line
    // if there are no runtime expressions.
    for (auto* node : executor.Program().ASTNodes().Objects()) {
        auto& loc = node->source.range.begin;
        if (auto* stmt = node->As<ast::Statement>()) {
            if (!stmt->Is<ast::BlockStatement>()) {
                // Add the statement if no other breakpoints are in the map for this line.
                possible_breakpoints_.insert({loc.line, stmt});
            }
        } else if (auto* expr = node->As<ast::Expression>()) {
            auto* sem = executor.Sem().GetVal(expr);
            if (!expr->Is<ast::IdentifierExpression>() && sem &&
                sem->Stage() == core::EvaluationStage::kRuntime) {
                // Add the expression if there is no other expression in the map for this line.
                auto prev = possible_breakpoints_.find(loc.line);
                if (prev == possible_breakpoints_.end() || prev->second->Is<ast::Statement>()) {
                    possible_breakpoints_.insert_or_assign(loc.line, expr);
                }
            }
        }
    }

    executor_.AddPreStepCallback([&](auto*) { return Interact(false); });
    executor_.AddErrorCallback([&](diag::List&& err) { return Error(std::move(err)); });
}

void InteractiveDebugger::Error(diag::List&& error) {
    diag::Formatter::Style style{};
    diag::Formatter formatter{style};
    diag_printer_->Print(formatter.Format(error));

    Interact(true);
}

void InteractiveDebugger::Interact(bool force_break) {
    if (!force_break) {
        auto* current_invocation = executor_.CurrentInvocation();
        const ast::Node* current_node = nullptr;
        if (current_invocation) {
            if (auto* expr = current_invocation->CurrentExpression()) {
                current_node = expr;
            } else if (auto* stmt = current_invocation->CurrentStatement()) {
                current_node = stmt;
            }
        }

        // Check if we have hit a breakpoint.
        if (breakpoints_.count(current_node)) {
            std::cout << "Hit breakpoint on line " << current_node->source.range.begin.line
                      << std::endl;
        } else {
            // Don't prompt if a 'continue' command was provided.
            if (continuing_) {
                return;
            }

            // Don't prompt if we are stepping over a whole statement.
            if (current_invocation && last_stmt) {
                if (current_invocation->CurrentStatement() == last_stmt) {
                    return;
                }
            }
        }
    }
    continuing_ = false;
    last_stmt = nullptr;

    ShowContext();

    // Loop until a command is entered that resumes execution or exits the session.
    while (true) {
        // Some commands can change the current invocation, so fetch this on every iteration.
        auto* current_invocation = executor_.CurrentInvocation();

        // Show a prompt and get the next line of user input.
        bool eof = false;
        std::string line;
#ifdef TINT_DEBUGGER_ENABLE_READLINE
        if (interactive_) {
            char* in = readline("(tint-interp) ");
            if (in) {
                line = in;
            } else {
                eof = true;
            }
        } else  // NOLINT(readability/braces)
#endif
        {
            if (interactive_) {
                std::cout << "(tint-interp) " << std::flush;
            }
            getline(input_, line);
            eof = input_.eof();
        }

        // Quit on EOF.
        if (eof) {
            std::cout << std::endl;
            exit(0);
        }

        // Repeat the last command when <enter> is pressed without any input.
        if (line.empty()) {
            line = last_command;
        } else {
            last_command = line;
#ifdef TINT_DEBUGGER_ENABLE_READLINE
            add_history(line.c_str());
#endif
        }

        // Split the command into space-delimited tokens.
        std::istringstream ss(line);
        std::vector<std::string> tokens{std::istream_iterator<std::string>{ss},
                                        std::istream_iterator<std::string>{}};

        if (tokens.empty()) {
            continue;
        }

        // Handle the command.
        if (tokens[0] == "h" || tokens[0] == "help") {
            // TODO(jrprice): Show more detailed command usage, e.g. with "help command".
            std::cout << R"(
Interactive commands:
  bt, backtrace    Show the current invocation's function call stack
  b, break         Set up a breakpoint on the specified line number
  br, breakpoint   List or modify existing breakpoints
  c, continue      Continue executing until the next breakpoint or error
  h, help          Show this message
  inv, invocation  Switch to the invocation with the specified local ID
  p, print         Print the value of a variable identifier
  q, quit          Exit this session
  s, step          Step the current invocation over the next statement
  se, stepe        Step the current invocation over the next expression
  wg, workgroup    Switch to the workgroup with the specified group ID
)";
            continue;
        } else if (tokens[0] == "bt" || tokens[0] == "backtrace") {
            Backtrace(tokens);
            continue;
        } else if (tokens[0] == "b" || tokens[0] == "break") {
            Break(tokens);
            continue;
        } else if (tokens[0] == "br" || tokens[0] == "breakpoint") {
            Breakpoint(tokens);
            continue;
        } else if (tokens[0] == "c" || tokens[0] == "continue") {
            continuing_ = true;
            return;
        } else if (tokens[0] == "inv" || tokens[0] == "invocation") {
            Invocation(tokens);
            continue;
        } else if (tokens[0] == "p" || tokens[0] == "print") {
            if (!current_invocation) {
                std::cerr << "No invocation currently running." << std::endl;
                continue;
            }
            if (tokens.size() < 2) {
                std::cerr << "Expected identifier for print command." << std::endl;
                continue;
            }
            auto value = current_invocation->GetValue(tokens[1]);
            std::cout << tokens[1] << " = " << value << std::endl;
            continue;
        } else if (tokens[0] == "q" || tokens[0] == "quit") {
            exit(0);
        } else if (tokens[0] == "s" || tokens[0] == "step" ||  //
                   tokens[0] == "se" || tokens[0] == "stepe") {
            if (!current_invocation) {
                std::cerr << "No invocation to step." << std::endl;
                continue;
            }
            auto state = current_invocation->GetState();
            if (state == Invocation::State::kFinished) {
                std::cerr << "Invocation has finished." << std::endl;
                continue;
            } else {
                if (tokens[0] == "s" || tokens[0] == "step") {
                    last_stmt = current_invocation->CurrentStatement();
                }
                break;
            }
        } else if (tokens[0] == "wg" || tokens[0] == "workgroup") {
            Workgroup(tokens);
            continue;
        } else {
            std::cerr << "Unrecognized interactive command '" << tokens[0] << "'" << std::endl;
        }
    }
}

namespace {
/// Get the current source range for `invocation` at the `frame`.
Source::Range GetCurrentSourceRange(const Invocation* invocation, uint32_t frame) {
    if (invocation == nullptr) {
        return {};
    }

    Source::Range source_loc;
    if (auto* expr = invocation->CurrentExpression(frame)) {
        source_loc = expr->source.range;
    } else if (auto* stmt = invocation->CurrentStatement(frame)) {
        source_loc = stmt->source.range;
        if (stmt->Is<ast::BlockStatement>()) {
            // The location is the opening brace of a block.
            source_loc.end = source_loc.begin;
            source_loc.end.column++;
        }
    } else if (auto* block = invocation->CurrentBlock(frame)) {
        // We are in a block but without a current statement, so we must be at the end.
        source_loc = Source::Range(block->source.range.end);
        source_loc.begin.column--;
    }
    return source_loc;
}
}  // namespace

void InteractiveDebugger::ShowBacktrace(uint32_t max_depth) const {
    auto* current_workgroup = executor_.CurrentWorkgroup();
    if (!current_workgroup) {
        std::cout << "<no execution context>" << std::endl;
        return;
    }
    std::cout << "* workgroup_id" << current_workgroup->GroupId().Str() << std::endl;

    auto* current_invocation = executor_.CurrentInvocation();
    if (!current_invocation) {
        return;
    }
    // Show the current invocation.
    auto& local = current_invocation->LocalInvocationId();
    auto& call_stack = current_invocation->GetCallStack();
    std::cout << "  * local_invocation_id" << local.Str() << std::endl;

    for (uint32_t i = 0; i < max_depth && i < call_stack.size(); i++) {
        // Indent the line and highlight the current frame.
        if (i == 0) {
            std::cout << "    * ";
        } else {
            std::cout << "      ";
        }

        // Show the frame number and function name.
        auto* frame = call_stack[call_stack.size() - i - 1].get();
        auto* func = frame->func;
        std::cout << "frame #" << i << ": " << func->name->symbol.Name() << "() at ";

        // Show the source location if available.
        auto source_loc = GetCurrentSourceRange(current_invocation, i);
        if (source_loc.begin.line == 0) {
            std::cout << "<no line information>" << std::endl;
        } else {
            {
                SCOPED_RICH_TEXT("\u001b[36m");
                std::cout << source_.path;
            }
            std::cout << ":";
            {
                SCOPED_RICH_TEXT("\u001b[33m");
                std::cout << source_loc.begin.line;
            }
            std::cout << ":";
            {
                SCOPED_RICH_TEXT("\u001b[33m");
                std::cout << source_loc.begin.column;
            }
            std::cout << std::endl;
        }
    }
}

void InteractiveDebugger::ShowContext() const {
    // Show the current invocation and function.
    ShowBacktrace(1);

    // Show source line information if available.
    auto source_loc = GetCurrentSourceRange(executor_.CurrentInvocation(), 0);
    if (source_loc.begin.line != 0) {
        // Show the current source line and three lines either side of it.
        ShowLines(static_cast<int64_t>(source_loc.begin.line) - 3, 7, source_loc);
    }
}
void InteractiveDebugger::ShowLines(int64_t first,
                                    int64_t count,
                                    Source::Range highlight /* = {}*/) const {
    auto& lines = source_.content.lines;
    int ln_chars = static_cast<int>(ceil(log10(static_cast<double>(lines.size() + 1))));

    for (int64_t line = first; line < first + count; line++) {
        // Skip line numbers that are outside the size of the source file.
        if (line < 1 || line > static_cast<int64_t>(lines.size())) {
            continue;
        }
        auto line_num = static_cast<size_t>(line);

        // Output an indicator for the highlighted line.
        if (highlight.begin.line == line_num) {
            SCOPED_RICH_TEXT("\u001b[36m");
            std::cout << "-> ";
        } else {
            std::cout << "   ";
        }

        // Output the line number, right-justified based on the largest line number in the file.
        {
            SCOPED_RICH_TEXT("\u001b[33m");
            std::cout << std::setfill(' ') << std::setw(ln_chars) << line_num << ":";
        }

        // Output the source line, highlighting a column range if necessary.
        if (!lines[line_num - 1].empty()) {
            if (highlight.begin.line == line_num) {
                size_t col_begin = highlight.begin.column;
                size_t col_end = highlight.end.column;
                if (col_begin == 0) {
                    col_begin = 1;
                }
                if (highlight.end.line != highlight.begin.line || col_end <= col_begin) {
                    // Multi-line range: just highlight to the end of the line.
                    col_end = lines[highlight.begin.line - 1].length() + 1;
                }

                auto& line_str = lines[line_num - 1];
                std::cout << " " << line_str.substr(0, col_begin - 1);
                {
                    SCOPED_RICH_TEXT("\u001b[7m");
                    std::cout << line_str.substr(col_begin - 1, col_end - col_begin);
                }
                std::cout << line_str.substr(col_end - 1);

                if (!use_rich_text_) {
                    // Output carets underneath for the highlighted region.
                    std::cout << std::endl;
                    std::cout << std::setw(ln_chars + static_cast<int>(col_begin) + 5);
                    for (size_t i = col_begin; i < col_end; i++) {
                        std::cout << "^";
                    }
                }
            } else {
                std::cout << " " << lines[line_num - 1];
            }
        }
        std::cout << std::endl;
    }
}

namespace {
std::optional<uint32_t> ParseU32(const std::string& str, const char* use) {
    uint32_t value = 0;
    const char* start = str.c_str();
    const char* end = start + str.length();
    auto result = std::from_chars(start, end, value, 10);
    if (result.ptr != end) {
        std::cerr << "Invalid " << use << " value '" << str << "'" << std::endl;
        return {};
    }
    return value;
}
}  // namespace

void InteractiveDebugger::Backtrace(const std::vector<std::string>& tokens) {
    if (tokens.size() > 2) {
        std::cerr << "Expected 'backtrace [max_depth]'" << std::endl;
        return;
    }

    // Parse the max_depth value if provided.
    uint32_t max_depth = UINT32_MAX;
    if (tokens.size() > 1) {
        // Parse the maximum depth as an integer.
        auto max_depth_input = ParseU32(tokens[1], "maximum depth");
        if (!max_depth_input) {
            return;
        }
        max_depth = max_depth_input.value();
    }

    ShowBacktrace(max_depth);
}

void InteractiveDebugger::Break(const std::vector<std::string>& tokens) {
    if (tokens.size() != 2) {
        std::cerr << "Expected 'break <line_number>'" << std::endl;
        return;
    }

    // Parse the line number as an integer.
    auto line_num = ParseU32(tokens[1], "line number");
    if (!line_num) {
        return;
    }

    // Check that the line contains a statement or expression that we can break on.
    auto itr = possible_breakpoints_.find(line_num.value());
    if (itr == possible_breakpoints_.end()) {
        std::cerr << "No statement or runtime expression on this line" << std::endl;
        return;
    }

    // Add the breakpoint.
    if (!breakpoints_.insert(itr->second).second) {
        std::cerr << "Breakpoint already exists at line " << line_num.value() << std::endl;
    } else {
        std::cout << "Breakpoint added at " << source_.path << ":" << line_num.value() << std::endl;
        ShowLines(static_cast<int64_t>(line_num.value()), 1, itr->second->source.range);
    }
}

void InteractiveDebugger::Breakpoint(const std::vector<std::string>& tokens) {
    auto show_help = []() {
        std::cout << "breakpoint list           List existing breakpoints" << std::endl;
        std::cout << "breakpoint clear <line>   Delete a breakpoint from the specified line"
                  << std::endl;
    };
    if (tokens.size() == 1) {
        show_help();
        return;
    }

    if (tokens[1] == "list") {
        std::cout << "Existing breakpoints:" << std::endl;
        for (auto* bp : breakpoints_) {
            ShowLines(static_cast<int64_t>(bp->source.range.begin.line), 1, bp->source.range);
        }
    } else if (tokens[1] == "clear") {
        if (tokens.size() != 3) {
            show_help();
            return;
        }

        // Parse the line number as an integer.
        auto line_num = ParseU32(tokens[2], "line number");
        if (!line_num) {
            return;
        }

        // Get the breakpoint location for this line.
        auto itr = possible_breakpoints_.find(line_num.value());
        if (itr == possible_breakpoints_.end() || !breakpoints_.count(itr->second)) {
            std::cerr << "No breakpoint on this line" << std::endl;
            return;
        }

        // Remove the breakpoint.
        if (breakpoints_.erase(itr->second)) {
            std::cout << "Breakpoint removed at " << source_.path << ":" << line_num.value()
                      << std::endl;
        }
    } else {
        std::cerr << "Invalid breakpoint command" << std::endl;
        show_help();
    }
}

void InteractiveDebugger::Invocation(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2 || tokens.size() > 4) {
        std::cerr << "Expected 'invocation local_id_x [local_id_y [local_id_z]]'" << std::endl;
        return;
    }

    auto* workgroup = executor_.CurrentWorkgroup();
    if (workgroup == nullptr) {
        std::cerr << "No workgroup currently executing." << std::endl;
        return;
    }

    // Parse the local invocation ID.
    std::optional<uint32_t> lx;
    std::optional<uint32_t> ly = 0;
    std::optional<uint32_t> lz = 0;
    lx = ParseU32(tokens[1], "local_id.x");
    if (tokens.size() > 2) {
        ly = ParseU32(tokens[2], "local_id.y");
    }
    if (tokens.size() > 3) {
        lz = ParseU32(tokens[3], "local_id.z");
    }
    if (!lx || !ly || !lz) {
        return;
    }
    UVec3 local_id(lx.value(), ly.value(), lz.value());

    // Check the ID is within the workgroup size.
    if (local_id.x >= workgroup->Size().x || local_id.y >= workgroup->Size().y ||
        local_id.z >= workgroup->Size().z) {
        std::cerr << "local_invocation_id" << local_id.Str() << " is not valid." << std::endl
                  << "Workgroup size: " << workgroup->Size().Str() << std::endl;
        return;
    }

    // Switch to it.
    if (!workgroup->SelectInvocation(local_id)) {
        std::cerr << "local_invocation_id" << local_id.Str()
                  << " has finished or is waiting at a barrier." << std::endl;
        return;
    }

    ShowContext();
}

void InteractiveDebugger::Workgroup(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2 || tokens.size() > 4) {
        std::cerr << "Expected 'workgroup group_id_x [group_id_y [group_id_z]]'" << std::endl;
        return;
    }

    // Parse the group ID.
    std::optional<uint32_t> gx;
    std::optional<uint32_t> gy = 0;
    std::optional<uint32_t> gz = 0;
    gx = ParseU32(tokens[1], "group_id.x");
    if (tokens.size() > 2) {
        gy = ParseU32(tokens[2], "group_id.y");
    }
    if (tokens.size() > 3) {
        gz = ParseU32(tokens[3], "group_id.z");
    }
    if (!gx || !gy || !gz) {
        return;
    }
    UVec3 group_id(gx.value(), gy.value(), gz.value());

    // Check the ID is inside the dispatch.
    if (group_id.x >= executor_.WorkgroupCount().x || group_id.y >= executor_.WorkgroupCount().y ||
        group_id.z >= executor_.WorkgroupCount().z) {
        std::cerr << "workgroup_id" << group_id.Str() << " is not in the dispatch." << std::endl
                  << "Total workgroup count: " << executor_.WorkgroupCount().Str() << std::endl;
        return;
    }

    // Switch to it.
    if (!executor_.SelectWorkgroup(group_id)) {
        std::cerr << "workgroup_id" << group_id.Str() << " has already finished." << std::endl;
        return;
    }

    ShowContext();
}

}  // namespace tint::interp
