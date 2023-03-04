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

#ifndef SRC_TINT_IR_BUILTINS_BUILTIN_H_
#define SRC_TINT_IR_BUILTINS_BUILTIN_H_

#include "src/tint/castable.h"
#include "src/tint/ir/call.h"
#include "src/tint/symbol_table.h"
#include "src/tint/type/type.h"
#include "src/tint/utils/string_stream.h"

namespace tint::ir::builtins {

/// A builtin instruction in the IR.
class Builtin : public Castable<Builtin, Call> {
  public:
    /// Constructor
    /// @param result the result value
    /// @param args the builtin arguments
    Builtin(Value* result, utils::VectorRef<Value*> args);
    Builtin(const Builtin& instr) = delete;
    Builtin(Builtin&& instr) = delete;
    ~Builtin() override;

    Builtin& operator=(const Builtin& instr) = delete;
    Builtin& operator=(Builtin&& instr) = delete;
};

}  // namespace tint::ir::builtins

#endif  // SRC_TINT_IR_BUILTINS_BUILTIN_H_
