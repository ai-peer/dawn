
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

#ifndef SRC_TINT_LANG_CORE_IR_TRAVERSE_H_
#define SRC_TINT_LANG_CORE_IR_TRAVERSE_H_

#include "src/tint/lang/core/ir/block.h"
#include "src/tint/utils/traits/traits.h"

namespace tint::core::ir {

/// Traverse calls @p callback with each instruction that matches the callback parameter type in @p
/// block and all child blocks.
/// @param block the block to traverse
/// @param callback a function with the signature `void(T*)`
template <typename CALLBACK>
void Traverse(Block* block, CALLBACK&& callback) {
    using T = std::remove_pointer_t<traits::ParameterType<CALLBACK, 0>>;

    Vector<ir::Block*, 8> queue;
    queue.Push(block);
    while (!queue.IsEmpty()) {
        for (auto* inst : *queue.Pop()) {
            if (auto* as_t = inst->As<T>()) {
                callback(as_t);
            }
            if (auto* ctrl = inst->As<ControlInstruction>()) {
                ctrl->ForeachBlock([&](ir::Block* child) { queue.Push(child); });
            }
        }
    }
}

}  // namespace tint::core::ir

#endif  // SRC_TINT_LANG_CORE_IR_TRAVERSE_H_
