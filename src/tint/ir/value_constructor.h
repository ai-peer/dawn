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

#ifndef SRC_TINT_IR_VALUE_CONSTRUCTOR_H_
#define SRC_TINT_IR_VALUE_CONSTRUCTOR_H_

#include "src/tint/castable.h"
#include "src/tint/ir/instruction.h"
#include "src/tint/symbol_table.h"
#include "src/tint/type/type.h"
#include "src/tint/utils/string_stream.h"

namespace tint::ir {

/// A value constructor instruction in the IR.
class ValueConstructor : public Castable<ValueConstructor, Instruction> {
  public:
    /// Constructor
    /// @param result the result value
    /// @param args the constructor arguments
    ValueConstructor(Value* result, utils::VectorRef<Value*> args);
    ValueConstructor(const ValueConstructor& instr) = delete;
    ValueConstructor(ValueConstructor&& instr) = delete;
    ~ValueConstructor() override;

    ValueConstructor& operator=(const ValueConstructor& instr) = delete;
    ValueConstructor& operator=(ValueConstructor&& instr) = delete;

    /// @returns the constructor arguments
    utils::VectorRef<Value*> Args() const { return args_; }

    /// Write the instruction to the given stream
    /// @param out the stream to write to
    /// @param st the symbol table
    /// @returns the stream
    utils::StringStream& ToString(utils::StringStream& out, const SymbolTable& st) const override;

  private:
    utils::Vector<Value*, 1> args_;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_VALUE_CONSTRUCTOR_H_
