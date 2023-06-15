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

#ifndef SRC_TINT_IR_RUNTIME_VALUE_H_
#define SRC_TINT_IR_RUNTIME_VALUE_H_

#include "src/tint/ir/value.h"
#include "src/tint/utils/string_stream.h"

namespace tint::ir {

/// Runtime value in the IR.
class RuntimeValue : public utils::Castable<RuntimeValue, Value> {
  public:
    /// Constructor
    /// @param type the type of the value
    explicit RuntimeValue(const type::Type* type);

    /// Destructor
    ~RuntimeValue() override;

    /// @returns the type of the value
    const type::Type* Type() override { return type_; }

  private:
    const type::Type* type_ = nullptr;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_RUNTIME_VALUE_H_
