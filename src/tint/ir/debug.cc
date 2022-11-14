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

#include "src/tint/ir/debug.h"

#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "src/tint/ir/block.h"
#include "src/tint/ir/if.h"
#include "src/tint/ir/loop.h"
#include "src/tint/ir/switch.h"
#include "src/tint/ir/terminator.h"
#include "src/tint/program.h"

namespace tint::ir {

// static
std::string Debug::AsDotGraph(const Module* mod) {
    size_t node_count = 0;

    std::unordered_set<const FlowNode*> visited;
    std::unordered_set<const FlowNode*> merge_nodes;
    std::unordered_map<const FlowNode*, std::string> node_to_name;
    std::stringstream out;

    auto name_for = [&](const FlowNode* node) -> std::string {
        if (node_to_name.count(node) > 0) {
            return node_to_name[node];
        }

        std::string name = "node_" + std::to_string(node_count);
        node_count += 1;

        node_to_name[node] = name;
        return name;
    };

    std::function<void(const FlowNode*)> Graph = [&](const FlowNode* node) {
        if (visited.count(node) > 0) {
            return;
        }
        visited.insert(node);

        tint::Switch(
            node,
            [&](const ir::Block* b) {
                if (node_to_name.count(b) == 0) {
                    out << name_for(b) << R"( [label="block"])" << std::endl;
                }
                out << name_for(b) << " -> " << name_for(b->branch_target);

                // Dashed lines to merge blocks
                if (merge_nodes.count(b->branch_target) != 0) {
                    out << " [style=dashed]";
                }

                out << std::endl;
                Graph(b->branch_target);
            },
            [&](const ir::Switch* s) {
                out << name_for(s) << R"( [label="switch"])" << std::endl;
                out << name_for(s->merge_target) << R"( [label="switch merge"])" << std::endl;
                merge_nodes.insert(s->merge_target);

                size_t i = 0;
                for (const auto& c : s->cases) {
                    out << name_for(c.start_target)
                        << R"( [label="case )" + std::to_string(i++) + R"("])" << std::endl;
                }
                out << name_for(s) << " -> {";
                for (const auto& c : s->cases) {
                    if (&c != &(s->cases[0])) {
                        out << ", ";
                    }
                    out << name_for(c.start_target);
                }
                out << "}" << std::endl;

                for (const auto& c : s->cases) {
                    Graph(c.start_target);
                }
                Graph(s->merge_target);
            },
            [&](const ir::If* i) {
                out << name_for(i) << R"( [label="if"])" << std::endl;
                out << name_for(i->true_target) << R"( [label="true"])" << std::endl;
                out << name_for(i->false_target) << R"( [label="false"])" << std::endl;
                out << name_for(i->merge_target) << R"( [label="if merge"])" << std::endl;
                merge_nodes.insert(i->merge_target);

                out << name_for(i) << " -> {";
                out << name_for(i->true_target) << ", " << name_for(i->false_target);
                out << "}" << std::endl;

                // Subgraph if true/false branches so they draw on the same line
                out << "subgraph sub_" << name_for(i) << " {" << std::endl;
                out << R"(rank="same")" << std::endl;
                out << name_for(i->true_target) << std::endl;
                out << name_for(i->false_target) << std::endl;
                out << "}" << std::endl;

                Graph(i->true_target);
                Graph(i->false_target);
                Graph(i->merge_target);
            },
            [&](const ir::Loop* l) {
                out << name_for(l) << R"( [label="loop"])" << std::endl;
                out << name_for(l->start_target) << R"( [label="start"])" << std::endl;
                out << name_for(l->continuing_target) << R"( [label="continuing"])" << std::endl;
                out << name_for(l->merge_target) << R"( [label="loop merge"])" << std::endl;
                merge_nodes.insert(l->merge_target);

                // Subgraph the continuing and merge so they get drawn on the same line
                out << "subgraph sub_" << name_for(l) << " {" << std::endl;
                out << R"(rank="same")" << std::endl;
                out << name_for(l->continuing_target) << std::endl;
                out << name_for(l->merge_target) << std::endl;
                out << "}" << std::endl;

                out << name_for(l) << " -> " << name_for(l->start_target) << std::endl;

                Graph(l->start_target);
                Graph(l->continuing_target);
                Graph(l->merge_target);
            },
            [&](const ir::Terminator*) {
                // Already done
            });
    };

    out << "digraph G {" << std::endl;
    for (const auto* func : mod->functions) {
        // Cluster each function to label and draw a box around it.
        out << "subgraph cluster_" << name_for(func) << " {" << std::endl;
        out << R"(label=")" << mod->program->Symbols().NameFor(func->source->symbol) << R"(")"
            << std::endl;
        out << name_for(func->start_target) << R"( [label="start"])" << std::endl;
        out << name_for(func->end_target) << R"( [label="end"])" << std::endl;
        Graph(func->start_target);
        out << "}" << std::endl;
    }
    out << "}";
    return out.str();
}

// static
std::string Debug::AsString(const Module* mod) {
    size_t node_count = 0;

    std::unordered_set<const FlowNode*> visited;
    std::unordered_map<const FlowNode*, std::string> node_to_name;
    std::stringstream out;

    auto name_for = [&](const FlowNode* node) -> std::string {
        if (node_to_name.count(node) > 0) {
            return node_to_name[node];
        }

        std::string name = "%bb_" + std::to_string(node_count);
        node_count += 1;

        node_to_name[node] = name;
        return name;
    };

    std::function<void(const Register&)> EmitReg = [&](const Register& reg) {
      out << "%r" << reg.id;
    };

    std::function<void(const Op::Data&)> EmitData = [&](const Op::Data& data) {
      if (data.HasConstant()) {
        auto& c = data.GetConstant();
        if (c.IsBool()) {
          out << c.AsBool();
        } else if (c.IsF16() || c.IsF32()) {
          out << c.AsF32();
        } else if (c.IsI32()) {
          out << c.AsI32();
        } else {
          out << c.AsU32();
        }
      } else {
        EmitReg(data.GetRegister());
      }
    };

    std::function<void(const utils::VectorRef<Op>)> EmitOps = [&](const utils::VectorRef<Op> ops) {
      for (auto& op : ops) {
        if (op.HasResult()) {
          EmitReg(op.result);
          out << " = ";
        }

        if (op.kind == Op::Kind::kLoadConstant) {
          EmitData(op.args[0]);
          continue;
        } else if (op.kind == Op::Kind::kLoad) {
          out << "LOAD";
          continue;
        } else if (op.kind == Op::Kind::kStore) {
          out << "STORE";
          continue;
        } else if (op.kind == Op::Kind::kCall) {
          out << "CALL";
          continue;
        }

        std::string name;
        switch (op.kind) {
          case Op::Kind::kAnd:
            name = "&";
            break;
          case Op::Kind::kOr:
            name = "|";
            break;
          case Op::Kind::kXor:
            name = "^";
            break;
          case Op::Kind::kLogicalAnd:
            name = "&&";
            break;
          case Op::Kind::kLogicalOr:
            name = "||";
            break;
          case Op::Kind::kEqual:
            name = "==";
            break;
          case Op::Kind::kNotEqual:
            name = "!=";
            break;
          case Op::Kind::kLessThan:
            name = "<";
            break;
          case Op::Kind::kLessThanEqual:
            name = "<=";
            break;
          case Op::Kind::kGreaterThan:
            name = ">";
            break;
          case Op::Kind::kGreaterThanEqual:
            name = ">=";
            break;
          case Op::Kind::kShiftLeft:
            name = "<<";
            break;
          case Op::Kind::kShiftRight:
            name = ">>";
            break;
          case Op::Kind::kAdd:
            name = "+";
            break;
          case Op::Kind::kSubtract:
            name = "-";
            break;
          case Op::Kind::kMultiply:
            name = "*";
            break;
          case Op::Kind::kDivide:
            name = "/";
              break;
          case Op::Kind::kModulo:
            name = "%";
            break;
          case Op::Kind::kNone:
          default: {
            out << "implement" << std::endl;
          }
        }
        out << std::endl;
      }
    };

    std::function<void(const FlowNode*)> Emit = [&](const FlowNode* node) {
        if (visited.count(node) > 0) {
            return;
        }
        visited.insert(node);

        tint::Switch(
            node,
            [&](const ir::Block* b) {
                out << name_for(b) << " = Block()" << std::endl;
                EmitOps(b->ops);
                Emit(b->branch_target);
            },
            [&](const ir::Switch* s) {
                out << name_for(s) << " = Switch(";
                EmitReg(s->condition);
                out << ")" << std::endl;

                for (const auto& c : s->cases) {
                    // TODO(dsinclair): Emit case selectors when known in IR
                    out << name_for(c.start_target) << " = Case()" << std::endl;
                    Emit(c.start_target);
                }
                out << "// Merge" << std::endl;
                Emit(s->merge_target);
            },
            [&](const ir::If* i) {
                out << name_for(i) << " = If(";
                EmitReg(i->condition);
                out << ")" << std::endl;

                out << "// True branch" << std::endl;
                Emit(i->true_target);
                out << "// False branch" << std::endl;
                Emit(i->false_target);
                out << "// Merge" << std::endl;
                Emit(i->merge_target);
            },
            [&](const ir::Loop* l) {
                out << name_for(l) << " = Loop()" << std::endl;
                Emit(l->start_target);
                out << "// Continue target" << std::endl;
                Emit(l->continuing_target);
                out << "// Merge" << std::endl;
                Emit(l->merge_target);
            },
            [&](const ir::Terminator*) {
                out << "// Terminator" << std::endl;
            });
    };

    for (const auto* func : mod->functions) {
        // Cluster each function to label and draw a box around it.
        out << "Function: " << mod->program->Symbols().NameFor(func->source->symbol) << std::endl;
        Emit(func->start_target);
        out << std::endl;
    }
    return out.str();
}

}  // namespace tint::ir
