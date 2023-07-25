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

#include "src/tint/lang/core/ir/transform/transform.h"

#include <iostream>

#include "src/tint/lang/core/ir/disassembler.h"
#include "src/tint/lang/core/ir/validator.h"

/// If set to 1 then the Tint will dump the IR after each transform.
#define TINT_DUMP_IR_AFTER_EACH_TRANSFORM 0

TINT_INSTANTIATE_TYPEINFO(tint::ir::transform::Transform);

namespace tint::ir::transform {

Transform::Transform() = default;
Transform::~Transform() = default;

void RunTransform([[maybe_unused]] Module* ir,
                  [[maybe_unused]] const char* name,
                  std::function<void()> transform) {
#ifndef NDEBUG
    auto result = Validate(*ir);
    if (!result) {
        std::cout << "=========================================================" << std::endl;
        std::cout << "== Validation before " << name << " failed:" << std::endl;
        std::cout << "=========================================================" << std::endl;
        std::cout << result.Failure().str();
        return;
    }
#endif

    transform();

#if TINT_DUMP_IR_AFTER_EACH_TRANSFORM
    Disassembler disasm(*ir);
    std::cout << "=========================================================" << std::endl;
    std::cout << "== Output of " << name << ":" << std::endl;
    std::cout << "=========================================================" << std::endl;
    std::cout << disasm.Disassemble();
#endif
}

}  // namespace tint::ir::transform
