// Copyright 2022 The Tint Authors.
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

#include "src/tint/ir/function_flow_node.h"

TINT_INSTANTIATE_TYPEINFO(tint::ir::FunctionFlowNode);

namespace tint::ir {

FunctionFlowNode::FunctionFlowNode(const ast::Function* f) : Base(), source(f) {}

FunctionFlowNode::~FunctionFlowNode() = default;

}  // namespace tint::ir
