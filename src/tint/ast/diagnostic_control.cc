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

#include "src/tint/ast/diagnostic_control.h"

#include "src/tint/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::DiagnosticControl);

namespace tint::ast {

DiagnosticControl::~DiagnosticControl() = default;

const DiagnosticControl* DiagnosticControl::Clone(CloneContext* ctx) const {
    auto src = ctx->Clone(source);
    auto rule = ctx->Clone(rule_name);
    return ctx->dst->create<DiagnosticControl>(src, severity, rule);
}

DiagnosticSeverity ParseDiagnosticSeverity(std::string_view str) {
    if (str == "error") {
        return DiagnosticSeverity::kError;
    }
    if (str == "warning") {
        return DiagnosticSeverity::kWarning;
    }
    if (str == "info") {
        return DiagnosticSeverity::kInfo;
    }
    if (str == "off") {
        return DiagnosticSeverity::kOff;
    }
    return DiagnosticSeverity::kUndefined;
}

std::ostream& operator<<(std::ostream& out, DiagnosticSeverity value) {
    switch (value) {
        case DiagnosticSeverity::kUndefined:
            return out << "<undefined>";
        case DiagnosticSeverity::kError:
            return out << "error";
        case DiagnosticSeverity::kWarning:
            return out << "warning";
        case DiagnosticSeverity::kInfo:
            return out << "info";
        case DiagnosticSeverity::kOff:
            return out << "off";
    }
    return out << "<unknown>";
}

}  // namespace tint::ast
