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

#ifndef SRC_TINT_LANG_SPIRV_IR_IO_VAR_H_
#define SRC_TINT_LANG_SPIRV_IR_IO_VAR_H_

#include "src/tint/lang/core/builtin_value.h"
#include "src/tint/lang/core/interpolation.h"
#include "src/tint/lang/core/ir/var.h"
#include "src/tint/utils/rtti/castable.h"

namespace tint::spirv::ir {

/// Attributes that can be applied to the IO variable.
struct IOVarAttributes {
    /// The value of a `@location` attribute.
    std::optional<uint32_t> location;
    /// The value of a `@index` attribute.
    std::optional<uint32_t> index;
    /// The value of a `@builtin` attribute.
    std::optional<core::BuiltinValue> builtin;
    /// The values of a `@interpolate` attribute.
    std::optional<core::Interpolation> interpolation;
    /// True if the variable is annotated with `@invariant`.
    bool invariant = false;
};

/// A shader IO var instruction in the SPIR-V dialect of the IR.
class IOVar : public Castable<IOVar, core::ir::Var> {
  public:
    /// Constructor
    /// @param result the result value
    /// @param attributes the IO attributes
    explicit IOVar(core::ir::InstructionResult* result, const IOVarAttributes& attributes);
    ~IOVar() override;

    /// @returns the IO attributes
    const IOVarAttributes& Attributes() { return attributes_; }

  private:
    IOVarAttributes attributes_;
};

}  // namespace tint::spirv::ir

#endif  // SRC_TINT_LANG_SPIRV_IR_IO_VAR_H_
