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

#include "tint/interp/invocation.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "tint/interp/memory.h"
#include "tint/interp/shader_executor.h"
#include "tint/lang/core/builtin_value.h"
#include "tint/lang/core/constant/scalar.h"
#include "tint/lang/core/type/abstract_float.h"
#include "tint/lang/core/type/abstract_int.h"
#include "tint/lang/core/type/i32.h"
#include "tint/lang/core/type/struct.h"
#include "tint/lang/wgsl/ast/assignment_statement.h"
#include "tint/lang/wgsl/ast/break_if_statement.h"
#include "tint/lang/wgsl/ast/call_expression.h"
#include "tint/lang/wgsl/ast/continue_statement.h"
#include "tint/lang/wgsl/ast/for_loop_statement.h"
#include "tint/lang/wgsl/ast/function.h"
#include "tint/lang/wgsl/ast/index_accessor_expression.h"
#include "tint/lang/wgsl/ast/int_literal_expression.h"
#include "tint/lang/wgsl/ast/let.h"
#include "tint/lang/wgsl/ast/loop_statement.h"
#include "tint/lang/wgsl/ast/member_accessor_expression.h"
#include "tint/lang/wgsl/ast/var.h"
#include "tint/lang/wgsl/ast/variable_decl_statement.h"
#include "tint/lang/wgsl/ast/while_statement.h"
#include "tint/lang/wgsl/sem/call.h"
#include "tint/lang/wgsl/sem/function.h"
#include "tint/lang/wgsl/sem/info.h"
#include "tint/lang/wgsl/sem/load.h"
#include "tint/lang/wgsl/sem/member_accessor_expression.h"
#include "tint/lang/wgsl/sem/type_expression.h"
#include "tint/lang/wgsl/sem/value_constructor.h"
#include "tint/lang/wgsl/sem/value_conversion.h"
#include "tint/lang/wgsl/sem/variable.h"
#include "tint/utils/rtti/switch.h"
#include "tint/utils/symbol/symbol_table.h"
#include "tint/utils/text/string_stream.h"

namespace tint::interp {

namespace {

/// Convert a core::constant::Value* to its string representation, recursively.
/// @param value the constant value
/// @param total_size the total size in bytes of the root value
/// @param offset the offset in bytes of this value from the start of the root value
/// @param indent the base amount of indentation to apply to each new line
/// @returns the string representation of the constant
std::string ToString(const core::constant::Value* value,
                     uint64_t total_size,
                     uint64_t offset,
                     int indent = 0) {
    // Helper to add a new line with the appropriate indentation.
    auto newline = [&indent]() {
        std::string result = "\n";
        for (int i = 0; i < indent; i++) {
            result += "  ";
        }
        return result;
    };

    return Switch(
        value->Type(),  //
        [&](const core::type::AbstractInt*) { return std::to_string(value->ValueAs<int64_t>()); },
        [&](const core::type::AbstractFloat*) { return std::to_string(value->ValueAs<double>()); },
        [&](const core::type::Bool*) { return value->ValueAs<bool>() ? "true" : "false"; },
        [&](const core::type::F32*) { return std::to_string(value->ValueAs<float>()); },
        [&](const core::type::F16*) { return std::to_string(value->ValueAs<float>()); },
        [&](const core::type::I32*) { return std::to_string(value->ValueAs<int32_t>()); },
        [&](const core::type::U32*) { return std::to_string(value->ValueAs<uint32_t>()); },
        [&](const core::type::Vector* vec) -> std::string {
            auto result = vec->FriendlyName();
            result += "{";
            for (uint32_t i = 0; i < vec->Width(); i++) {
                if (i > 0) {
                    result += ", ";
                }
                result += ToString(value->Index(i), total_size, offset + i * vec->type()->Size());
            }
            result += "}";
            return result;
        },
        [&](const core::type::Matrix* mat) -> std::string {
            auto result = mat->FriendlyName();
            result += "{";
            indent++;
            for (uint32_t i = 0; i < mat->columns(); i++) {
                result += newline();
                result += ToString(value->Index(i), total_size, offset + i * mat->ColumnStride());
                result += ",";
            }
            indent--;
            result += newline() + "}";
            return result;
        },
        [&](const core::type::Array* arr) -> std::string {
            uint64_t count;
            if (arr->Count()->Is<core::type::RuntimeArrayCount>()) {
                count = (total_size - offset) / arr->Stride();
            } else if (arr->Count()->Is<sem::NamedOverrideArrayCount>() ||
                       arr->Count()->Is<sem::UnnamedOverrideArrayCount>()) {
                TINT_ASSERT(offset == 0);
                count = total_size / arr->Stride();
            } else if (arr->ConstantCount()) {
                count = arr->ConstantCount().value();
            } else {
                return "<unimplemented non-constant array size>";
            }
            auto result = arr->FriendlyName();
            result += "{";
            indent++;
            for (uint32_t i = 0; i < count; i++) {
                result += newline() + "[" + std::to_string(i) + "] = ";
                result += ToString(value->Index(i), total_size, offset + i * arr->Stride(), indent);
                result += ",";
            }
            indent--;
            result += newline() + "}";
            return result;
        },
        [&](const core::type::Struct* str) -> std::string {
            auto result = str->FriendlyName();
            result += "{";
            indent++;
            for (auto* member : str->Members()) {
                result += newline() + "." + member->Name().Name() + " = ";
                result += ToString(value->Index(member->Index()), total_size,
                                   offset + member->Offset(), indent);
                result += ",";
            }
            indent--;
            result += newline() + "}";
            return result;
        },
        [&](Default) { return "<unimplemented value type>"; });
}

}  // namespace

Invocation::Invocation(ShaderExecutor& executor,
                       UVec3 group_id,
                       UVec3 local_id,
                       const std::unordered_map<const sem::Variable*, MemoryView*>& allocations)
    : executor_(executor),
      workgroup_id_(group_id),
      local_invocation_id_(local_id),
      local_invocation_index_(local_id.x +
                              executor.WorkgroupSize().x *
                                  (local_id.y + executor.WorkgroupSize().y * local_id.z)) {
    // Set up module-scope global variables.
    auto* func = executor_.Sem().Get(executor_.EntryPoint());
    auto& bindings = executor_.Bindings();
    for (auto* global : func->TransitivelyReferencedGlobals()) {
        // Register the variable's identifier in the global scope.
        auto ident = global->Declaration()->name->symbol.Name();
        global_identifiers_.Set(ident, global);

        // Skip constants and pipeline-overrides.
        if (global->Stage() <= core::EvaluationStage::kOverride) {
            continue;
        }

        auto* store_type = global->Type()->UnwrapRef();
        MemoryView* view = nullptr;
        switch (global->AddressSpace()) {
            case core::AddressSpace::kStorage:
            case core::AddressSpace::kUniform: {
                if (!bindings.count(global)) {
                    executor_.ReportFatalError("missing resource binding",
                                               global->Declaration()->source);
                    return;
                }
                view = bindings.at(global);
                break;
            }
            case core::AddressSpace::kWorkgroup: {
                TINT_ASSERT(allocations.count(global));
                view = allocations.at(global);
                break;
            }
            case core::AddressSpace::kPrivate: {
                // Create a memory allocation and a view into it.
                auto alloc = std::make_unique<Memory>(store_type->Size());
                view = alloc->CreateView(&executor, global->AddressSpace(), store_type,
                                         global->Declaration()->source);
                private_allocations_.push_back(std::move(alloc));

                // Store the value of the initializer.
                const core::constant::Value* init_value;
                if (global->Initializer()) {
                    if (!global->Initializer()->ConstantValue()) {
                        executor_.ReportFatalError(
                            "unhandled non-constant module-scope initializer",
                            global->Declaration()->source);
                        return;
                    }
                    init_value = global->Initializer()->ConstantValue();
                } else {
                    // Generate a zero-init value when no initializer is provided.
                    auto zero = executor_.ConstEval().Zero(store_type, {}, {});
                    if (zero != Success) {
                        executor_.ReportFatalError("zero initializer generation failed",
                                                   global->Declaration()->source);
                        return;
                    }
                    init_value = zero.Get();
                }
                view->Store(init_value);
                break;
            }
            default:
                executor_.ReportFatalError("unhandled global variable address space",
                                           global->Declaration()->source);
                return;
        }

        // Store the reference created for the variable.
        variable_values_[global] = ExprResult::MakeReference(view);
    }

    // Helpers for creating constants to represent builtin values.
    auto& wgsize = executor_.WorkgroupSize();
    auto& num_groups = executor_.WorkgroupCount();
    auto* u32_ty = executor_.Builder().Types().Get<core::type::U32>();
    auto* uvec3_ty = executor_.Builder().Types().Get<core::type::Vector>(u32_ty, 3u);
    auto make_u32 = [&](uint32_t value) {
        return executor_.Builder().constants.Get(core::u32(value));
    };
    auto make_uvec3 = [&](const Source& source, uint32_t a, uint32_t b, uint32_t c) {
        tint::Vector<const core::constant::Value*, 3> els;
        els.Push(make_u32(a));
        els.Push(make_u32(b));
        els.Push(make_u32(c));
        return executor_.ConstEval().VecInitS(uvec3_ty, els, source).Get();
    };
    auto get_builtin = [&](auto* node) -> const core::constant::Value* {
        if (auto* builtin = ast::GetAttribute<ast::BuiltinAttribute>(node->attributes)) {
            switch (executor_.Sem().Get(builtin)->Value()) {
                case core::BuiltinValue::kGlobalInvocationId:
                    return make_uvec3(node->source, local_id.x + group_id.x * wgsize.x,
                                      local_id.y + group_id.y * wgsize.y,
                                      local_id.z + group_id.z * wgsize.z);
                case core::BuiltinValue::kLocalInvocationId:
                    return make_uvec3(node->source, local_id.x, local_id.y, local_id.z);
                case core::BuiltinValue::kLocalInvocationIndex:
                    return make_u32(local_invocation_index_);
                case core::BuiltinValue::kNumWorkgroups:
                    return make_uvec3(node->source, num_groups.x, num_groups.y, num_groups.z);
                case core::BuiltinValue::kWorkgroupId:
                    return make_uvec3(node->source, group_id.x, group_id.y, group_id.z);
                default:
                    executor_.ReportFatalError("unhandled entry point builtin", node->source);
                    return nullptr;
            }
        } else {
            executor_.ReportFatalError("unhandled entry point parameter", node->source);
            return nullptr;
        }
    };

    // Set up entry point parameters.
    tint::Vector<ExprResult, 4> args;
    for (auto* param : func->Parameters()) {
        if (auto* str = param->Type()->As<sem::Struct>()) {
            tint::Vector<const core::constant::Value*, 4> members;
            for (auto* member : str->Members()) {
                members.Push(get_builtin(member->Declaration()));
            }
            args.Push(
                ExprResult::MakeValue(executor_.ConstEval().ArrayOrStructCtor(str, members).Get()));
        } else {
            args.Push(ExprResult::MakeValue(get_builtin(param->Declaration())));
        }
    }

    StartFunction(executor_.EntryPoint(), args);
}

Invocation::Invocation(ShaderExecutor& executor) : executor_(executor) {}

Invocation::~Invocation() {}

Invocation::State Invocation::GetState() const {
    if (call_stack_.empty()) {
        return State::kFinished;
    }
    if (barrier_ != nullptr) {
        return State::kBarrier;
    }
    return State::kReady;
}

void Invocation::Step() {
    if (call_stack_.empty()) {
        std::cerr << "Trying to step an invocation that has already finished." << std::endl;
        return;
    }

    auto* current_function = call_stack_.back().get();
    TINT_ASSERT(!current_function->block_stack.empty());
    auto& current_block = current_function->block_stack.top();
    if (current_block.current_statement == current_block.block->statements.end()) {
        EndBlock();
        return;
    }

    if (current_block.next_expr < current_block.expr_queue.size()) {
        // Evaluate the next expression in the expression queue.

        // If the next expression is the RHS of a short-circuiting operator, we check the value of
        // the evaluated LHS result to see if we should skip evaluation of the RHS.
        if (current_block.short_circuiting_ops.count(current_block.next_expr)) {
            uint32_t op_idx = current_block.short_circuiting_ops.at(current_block.next_expr);
            auto* binop = current_block.expr_queue[op_idx].expr->As<ast::BinaryExpression>();
            TINT_ASSERT(binop && binop->IsLogical());
            bool lhs = GetResult(binop->lhs).Value()->ValueAs<bool>();
            if (binop->IsLogicalOr() == lhs) {
                // Jump to the index of the short-circuiting operator.
                current_block.next_expr = op_idx;
            }
        }

        // Evaluate the next expression.
        auto& next = current_block.expr_queue[current_block.next_expr];
        auto result = next.func();
        executor_.FlushErrors();

        if (call_stack_.back()->func != current_function->func) {
            // We've changed function, so nothing more to do for this step.
            return;
        }

        // Apply the load rule if necessary.
        if (executor_.Sem().Get<sem::Load>(next.expr)) {
            result = ExprResult::MakeValue(result.MemoryView()->Load());
        }

        current_block.expr_results[next.expr] = result;
        current_block.next_expr++;
    } else {
        // Execute the statement, as all of its expression dependencies have been evaluated.
        TINT_ASSERT(current_block.current_stmt_exec);
        auto stmt = *current_block.current_statement;
        auto stmt_exec = current_block.current_stmt_exec;
        current_block.current_stmt_exec = nullptr;

        stmt_exec();

        // Make sure the invocation has advanced to a new statement.
        TINT_ASSERT(CurrentStatement() != stmt);
    }
}

void Invocation::ClearBarrier() {
    auto& current_block = call_stack_.back()->block_stack.top();
    auto* builtin = executor_.Sem().Get<sem::Call>(barrier_)->Target()->As<sem::BuiltinFn>();
    if (builtin->Fn() == wgsl::BuiltinFn::kWorkgroupUniformLoad) {
        // Load the value through the pointer argument.
        auto* ptr = GetResult(barrier_->args[0]).Pointer();
        current_block.expr_results[barrier_] = ExprResult::MakeValue(ptr->Load());
    }

    barrier_ = nullptr;
}

const ast::BlockStatement* Invocation::CurrentBlock(uint32_t frame /* = 0 */) const {
    if (frame >= call_stack_.size()) {
        return nullptr;
    }
    auto& entry = call_stack_[call_stack_.size() - frame - 1];
    if (entry->block_stack.empty()) {
        return nullptr;
    }
    return entry->block_stack.top().block;
}

const ast::Statement* Invocation::CurrentStatement(uint32_t frame /* = 0 */) const {
    if (frame >= call_stack_.size()) {
        return nullptr;
    }
    auto& entry = call_stack_[call_stack_.size() - frame - 1];
    if (entry->block_stack.empty()) {
        return nullptr;
    }

    auto& current_block = entry->block_stack.top();
    if (current_block.current_statement == current_block.block->statements.end()) {
        return nullptr;
    }
    return *current_block.current_statement;
}

const ast::Expression* Invocation::CurrentExpression(uint32_t frame /* = 0 */) const {
    if (frame >= call_stack_.size()) {
        return nullptr;
    }
    auto& entry = call_stack_[call_stack_.size() - frame - 1];
    if (entry->block_stack.empty()) {
        return nullptr;
    }

    auto& current_block = entry->block_stack.top();
    if (current_block.next_expr < current_block.expr_queue.size()) {
        return current_block.expr_queue[current_block.next_expr].expr;
    }
    return nullptr;
}

std::string Invocation::GetValue(std::string identifier) const {
    if (call_stack_.empty()) {
        return "<invocation not running>";
    }

    // Get the semantic variable corresponding to identifier in the current scope.
    auto* var = call_stack_.back()->identifiers.Get(identifier);
    if (!var) {
        return "<identifier not found>";
    }

    switch (var->Stage()) {
        case core::EvaluationStage::kConstant: {
            auto* value = var->ConstantValue();
            return tint::interp::ToString(value, value->Type()->Size(), 0);
        }
        case core::EvaluationStage::kOverride: {
            auto* value = executor_.GetNamedOverride(var);
            return tint::interp::ToString(value, value->Type()->Size(), 0);
        }
        case core::EvaluationStage::kRuntime: {
            if (!variable_values_.count(var)) {
                return "<missing variable value>";
            }
            break;
        }
        default: {
            return "<invalid variable evaluation stage>";
        }
    }

    // Get the value of the variable.
    auto expr = variable_values_.at(var);
    switch (expr.Kind()) {
        case ExprResult::Kind::kReference: {
            // Load the value and convert it to a string.
            return tint::interp::ToString(expr.Reference()->Load(), expr.Reference()->Size(), 0);
        }
        case ExprResult::Kind::kValue: {
            // Convert the value to a string.
            return tint::interp::ToString(expr.Value(), expr.Value()->Type()->Size(), 0);
        }
        case ExprResult::Kind::kPointer: {
            tint::StringStream ss;
            auto* view = expr.Pointer();
            ss << "ptr<" << view->AddressSpace() << ", " << view->Type()->FriendlyName() << ">";
            return ss.str();
        }
        case ExprResult::Kind::kInvalid: {
            return "<expression produced invalid result>";
        }
    }
    return "<invalid>";
}

void Invocation::StartFunction(const ast::Function* func, tint::VectorRef<ExprResult> args) {
    call_stack_.push_back(std::make_unique<CallStackEntry>(func));
    call_stack_.back()->identifiers = global_identifiers_;

    // Copy parameter values to this function variable and identifier scope.
    TINT_ASSERT(func->params.Length() == args.Length());
    auto* sem_func = executor_.Sem().Get(func);
    for (auto* param : sem_func->Parameters()) {
        variable_values_[param] = args[param->Index()];
        auto ident = param->Declaration()->name->symbol.Name();
        call_stack_.back()->identifiers.Set(ident, param);
    }

    StartBlock(func->body);
}

void Invocation::StartBlock(const ast::BlockStatement* block) {
    auto* func = call_stack_.back().get();
    func->block_stack.push({block, block->statements.begin()});
    func->identifiers.Push();

    // Prepare to execute the first statement in the block.
    if (!block->statements.IsEmpty()) {
        func->block_stack.top().current_stmt_exec = PrepareStatement(block->statements.Front());
    }
}

void Invocation::EndBlock(BlockEndKind kind /* = BlockEndKind::kRegular */) {
    // Pop back up the block stack.
    auto* current_function = call_stack_.back().get();
    auto* prev_block = current_function->block_stack.top().block;
    current_function->block_stack.pop();
    current_function->identifiers.Pop();
    if (current_function->block_stack.empty()) {
        // We reached the end of the current function, so just return.
        ReturnFromFunction();
        return;
    }

    auto& parent_block = current_function->block_stack.top();

    // Handle the cases where we are ending a loop or switch statement.
    Switch(
        *parent_block.current_statement,  //
        [&](const ast::ForLoopStatement* for_loop) {
            if (kind == BlockEndKind::kBreak) {
                NextStatement();
            } else if (for_loop->continuing) {
                // Switch to the continuing statement.
                auto* loop_stmt_itr = parent_block.current_statement;
                parent_block.current_statement = &for_loop->continuing;
                auto exec_continuing = PrepareStatement(for_loop->continuing);

                // Execute the continuing statement and then switch back to the main loop statement
                // to evaluate the condition.
                parent_block.current_stmt_exec = [&, for_loop, exec_continuing, loop_stmt_itr]() {
                    exec_continuing();
                    parent_block.current_statement = loop_stmt_itr;
                    parent_block.current_stmt_exec =
                        LoopCondition(for_loop->condition, for_loop->body);
                };
            } else {
                // No continuing statement, so go straight to the condition.
                parent_block.current_stmt_exec = LoopCondition(for_loop->condition, for_loop->body);
            }
        },
        [&](const ast::LoopStatement* loop) {
            if (kind == BlockEndKind::kBreak) {
                NextStatement();
            } else if (loop->continuing && loop->continuing != prev_block &&
                       !loop->continuing->Empty()) {
                StartBlock(loop->continuing);
            } else {
                StartBlock(loop->body);
            }
        },
        [&](const ast::WhileStatement* loop) {
            if (kind == BlockEndKind::kBreak) {
                NextStatement();
            } else {
                parent_block.current_stmt_exec = LoopCondition(loop->condition, loop->body);
            }
        },
        [&](const ast::SwitchStatement*) {
            if (kind == BlockEndKind::kRegular || kind == BlockEndKind::kBreak) {
                NextStatement();
            } else {
                // Pop up the block stack until we hit something that handles this kind of end.
                EndBlock(kind);
            }
        },
        [&](Default) {
            if (kind != BlockEndKind::kRegular) {
                // Pop up the block stack until we hit something that handles this kind of end.
                EndBlock(kind);
            } else {
                // Move to the next statement from the parent block.
                NextStatement();
            }
        });
}

void Invocation::ReturnFromFunction(ExprResult return_value) {
    call_stack_.pop_back();

    // Return the return value to the caller if needed.
    if (!call_stack_.empty()) {
        auto& current_block = call_stack_.back()->block_stack.top();
        if (return_value.Kind() != ExprResult::Kind::kInvalid) {
            current_block.expr_results[current_block.expr_queue[current_block.next_expr].expr] =
                return_value;
        }
        current_block.next_expr++;
    }
}

Invocation::StmtExecutor Invocation::PrepareStatement(const ast::Statement* stmt) {
    auto& current_block = call_stack_.back()->block_stack.top();
    current_block.expr_queue.clear();
    current_block.expr_results.clear();
    current_block.short_circuiting_ops.clear();
    current_block.next_expr = 0;
    return Switch(
        stmt,  //
        [&](const ast::AssignmentStatement* assign) { return Assignment(assign); },
        [&](const ast::BlockStatement* block) { return [&, block]() { StartBlock(block); }; },
        [&](const ast::BreakStatement* brk) { return Break(brk); },
        [&](const ast::BreakIfStatement* brk) { return BreakIf(brk); },
        [&](const ast::CallStatement* call) { return CallStmt(call); },
        [&](const ast::CompoundAssignmentStatement* assign) { return CompoundAssignment(assign); },
        [&](const ast::ConstAssert*) { return [&]() { NextStatement(); }; },
        [&](const ast::ContinueStatement* cont) { return Continue(cont); },
        [&](const ast::ForLoopStatement* loop) { return ForLoop(loop); },
        [&](const ast::IfStatement* if_stmt) { return If(if_stmt); },
        [&](const ast::IncrementDecrementStatement* inc_dec) {
            return IncrementDecrement(inc_dec);
        },
        [&](const ast::LoopStatement* loop) { return Loop(loop); },
        [&](const ast::ReturnStatement* ret) { return ReturnStmt(ret); },
        [&](const ast::SwitchStatement* swtch) { return SwitchStmt(swtch); },
        [&](const ast::VariableDeclStatement* decl) { return VariableDecl(decl); },
        [&](const ast::WhileStatement* loop) { return While(loop); },
        [&](Default) {
            return [&, stmt]() {
                executor_.ReportFatalError("unhandled statement type", stmt->source);
            };
        });
}

void Invocation::EnqueueExpression(const ast::Expression* expr) {
    if (call_stack_.empty() || call_stack_.back()->block_stack.empty()) {
        executor_.ReportFatalError("enqueuing expression outside of a function");
        return;
    }

    auto& current_block = call_stack_.back()->block_stack.top();

    // If the expression is null, there is nothing to do.
    if (expr == nullptr) {
        return;
    }

    // If the expression already has a constant value, just use that.
    auto* sem = executor_.Sem().GetVal(expr);
    if (auto* value = sem->ConstantValue()) {
        current_block.expr_results[expr] = ExprResult::MakeValue(value);
        return;
    }

    // The expression needs to be evaluated, so add it to the queue.
    auto eval = Switch(
        expr,  //
        [&](const ast::BinaryExpression* binary) { return EnqueueBinary(binary); },
        [&](const ast::CallExpression* call) { return EnqueueCall(call); },
        [&](const ast::IdentifierExpression* ident) { return EnqueueIdentifier(ident); },
        [&](const ast::IndexAccessorExpression* index) { return EnqueueIndexAccessor(index); },
        [&](const ast::MemberAccessorExpression* access) { return EnqueueMemberAccessor(access); },
        [&](const ast::PhonyExpression*) { return [&]() { return ExprResult(); }; },
        [&](const ast::UnaryOpExpression* access) { return EnqueueUnaryOp(access); },
        [&](Default) {
            return [&, expr]() {
                executor_.ReportFatalError("unhandled expression type", expr->source);
                return ExprResult();
            };
        });
    current_block.expr_queue.push_back({expr, eval});
}

ExprResult Invocation::GetResult(const ast::Expression* expr) {
    if (call_stack_.empty() || call_stack_.back()->block_stack.empty()) {
        executor_.ReportFatalError("getting expression result outside of a function");
        return ExprResult();
    }
    auto& results = call_stack_.back()->block_stack.top().expr_results;
    if (!results.count(expr)) {
        executor_.ReportFatalError("expression result not found");
        return ExprResult();
    }
    return results.at(expr);
}

Invocation::ExprEvaluator Invocation::EnqueueBinary(const ast::BinaryExpression* binary) {
    auto& current_block = call_stack_.back()->block_stack.top();

    EnqueueExpression(binary->lhs);
    uint32_t rhs_expr_idx = static_cast<uint32_t>(current_block.expr_queue.size());
    EnqueueExpression(binary->rhs);

    // Special-case short-circuiting operators to skip the RHS if necessary.
    if (binary->IsLogical()) {
        // Register the short-circuiting operator by adding a map entry that maps from the index
        // of the start of the RHS to the index of this operator.
        current_block.short_circuiting_ops[rhs_expr_idx] =
            static_cast<uint32_t>(current_block.expr_queue.size());
        return [&, binary]() {
            // If the LHS result means we should short-circuit, just return true/false directly.
            // Otherwise, return the result of the RHS.
            bool result;
            auto* lhs = GetResult(binary->lhs).Value();
            if (binary->IsLogicalAnd()) {
                if (!lhs->ValueAs<bool>()) {
                    result = false;
                } else {
                    result = GetResult(binary->rhs).Value()->ValueAs<bool>();
                }
            } else {
                if (lhs->ValueAs<bool>()) {
                    result = true;
                } else {
                    result = GetResult(binary->rhs).Value()->ValueAs<bool>();
                }
            }
            return ExprResult::MakeValue(executor_.Builder().constants.Get(result));
        };
    }

    return [&, binary]() {
        auto args = tint::Vector{GetResult(binary->lhs).Value(), GetResult(binary->rhs).Value()};
        auto op = executor_.IntrinsicTable().Lookup(binary->op, args[0]->Type(), args[1]->Type(),
                                                    core::EvaluationStage::kConstant, false);
        TINT_ASSERT(op == Success);
        auto result =
            (executor_.ConstEval().*op->const_eval_fn)(op->return_type, args, binary->source);
        return ExprResult::MakeValue(result.Get());
    };
}

Invocation::ExprEvaluator Invocation::EnqueueCall(const ast::CallExpression* call) {
    // Enqueue evaluation of argument expressions.
    for (auto* arg : call->args) {
        EnqueueExpression(arg);
    }

    return [&, call]() {
        auto* sem_call = executor_.Sem().Get<sem::Call>(call);
        auto* target = sem_call->Target();
        auto* result_ty = sem_call->Type();
        return Switch(
            target,  //
            [&](const sem::Function* user_func) {
                // Prepare call arguments.
                tint::Vector<ExprResult, 4> args;
                for (auto* arg : call->args) {
                    args.Push(GetResult(arg));
                }

                // Switch execution to the target function.
                StartFunction(user_func->Declaration(), std::move(args));

                // This is a placeholder, and will be overwritten by the callee on return.
                return ExprResult();
            },
            [&](const sem::ValueConversion*) {
                auto* arg = GetResult(call->args[0]).Value();
                auto result = executor_.ConstEval().Convert(result_ty, arg, call->source);
                if (result != Success) {
                    executor_.ReportFatalError("type conversion failed", call->source);
                    return ExprResult();
                }
                return ExprResult::MakeValue(result.Get());
            },
            [&](const sem::ValueConstructor*) {
                // Prepare type constructor arguments.
                tint::Vector<const core::constant::Value*, 4> arg_values;
                tint::Vector<const core::type::Type*, 4> arg_types;
                for (auto* arg : call->args) {
                    auto* arg_result = GetResult(arg).Value();
                    arg_values.Push(arg_result);
                    arg_types.Push(arg_result->Type());
                }

                // Helper to lookup and call a matrix or vector constructor's const eval function.
                auto mat_vec = [&](const core::type::Type* type,
                                   wgsl::intrinsic::CtorConv intrinsic) {
                    auto op = executor_.IntrinsicTable().Lookup(intrinsic, Vector{type}, arg_types,
                                                                core::EvaluationStage::kConstant);
                    if (!op->const_eval_fn) {
                        executor_.ReportFatalError("unhandled type constructor", call->source);
                        return core::constant::Eval::Result(nullptr);
                    }
                    return (executor_.ConstEval().*op->const_eval_fn)(sem_call->Type(), arg_values,
                                                                      call->source);
                };

                // Dispatch to the appropriate const eval function.
                auto result = Switch(
                    result_ty,  //
                    [&](const core::type::Array*) {
                        return executor_.ConstEval().ArrayOrStructCtor(result_ty, arg_values);
                    },
                    [&](const core::type::Struct*) {
                        return executor_.ConstEval().ArrayOrStructCtor(result_ty, arg_values);
                    },
                    [&](const core::type::Vector* vec) {
                        return mat_vec(vec->type(), wgsl::intrinsic::VectorCtorConv(vec->Width()));
                    },
                    [&](const core::type::Matrix* mat) {
                        return mat_vec(mat->type(), wgsl::intrinsic::MatrixCtorConv(mat->columns(),
                                                                                    mat->rows()));
                    },
                    [&](Default) {
                        if (!result_ty->Is<core::type::Scalar>()) {
                            executor_.ReportFatalError("unhandled type constructor", call->source);
                            return core::constant::Eval::Result(nullptr);
                        }
                        // For scalars, this must be an identity constructor.
                        if (arg_values[0]->Type() != result_ty) {
                            executor_.ReportFatalError("invalid type constructor", call->source);
                            return core::constant::Eval::Result(nullptr);
                        }
                        return executor_.ConstEval().Identity(result_ty, arg_values, call->source);
                    });

                if (result != Success) {
                    executor_.ReportFatalError("type construction failed", call->source);
                    return ExprResult();
                }
                return ExprResult::MakeValue(result.Get());
            },
            [&](const sem::BuiltinFn* builtin) { return EvaluateBuiltin(builtin, call); },
            [&](Default) -> ExprResult {
                executor_.ReportFatalError("unhandled call expression target", call->source);
                return ExprResult();
            });
    };
}

ExprResult Invocation::EvaluateBuiltin(const sem::BuiltinFn* builtin,
                                       const ast::CallExpression* call) {
    // Get explicit template argument types.
    Vector<const core::type::Type*, 1> tmpl_types;
    if (auto* tmpl = call->target->identifier->As<ast::TemplatedIdentifier>()) {
        for (auto* arg : tmpl->arguments) {
            auto* arg_ty = executor_.Sem().Get(arg)->As<sem::TypeExpression>();
            tmpl_types.Push(arg_ty->Type());
        }
    }

    // Get argument types.
    tint::Vector<const core::type::Type*, 4> arg_types;
    for (auto* arg : call->args) {
        arg_types.Push(executor_.Sem().GetVal(arg)->Type());
    }

    // Check for a const-eval implementation of this builtin.
    auto op = executor_.IntrinsicTable().Lookup(builtin->Fn(), tmpl_types, arg_types,
                                                core::EvaluationStage::kConstant);
    if (op->const_eval_fn) {
        // Get the argument values.
        tint::Vector<const core::constant::Value*, 4> arg_values;
        for (auto* arg : call->args) {
            auto* arg_result = GetResult(arg).Value();
            arg_values.Push(arg_result);
        }

        // Call the const-eval function.
        auto result = (executor_.ConstEval().*op->const_eval_fn)(builtin->ReturnType(), arg_values,
                                                                 call->source);
        if (result != Success) {
            executor_.ReportFatalError("builtin call evaluation failed", call->source);
            return ExprResult();
        }
        return ExprResult::MakeValue(result.Get());
    }
    if (builtin->Fn() == wgsl::BuiltinFn::kArrayLength) {
        auto ptr = GetResult(call->args[0]);
        auto* arr = ptr.Pointer()->Type()->As<core::type::Array>();
        auto* result =
            executor_.Builder().constants.Get(core::u32(ptr.Pointer()->Size() / arr->Stride()));
        return ExprResult::MakeValue(result);
    }
    if (builtin->IsAtomic()) {
        return EvaluateBuiltinAtomic(builtin, call);
    }
    if (builtin->Fn() == wgsl::BuiltinFn::kStorageBarrier ||
        builtin->Fn() == wgsl::BuiltinFn::kWorkgroupBarrier) {
        barrier_ = call;
        return ExprResult();
    }
    if (builtin->Fn() == wgsl::BuiltinFn::kWorkgroupUniformLoad) {
        barrier_ = call;
        // This is a placeholder that will be overwritten when the workgroup clears the barrier.
        return ExprResult();
    }

    executor_.ReportFatalError("unhandled builtin call", call->source);
    return ExprResult();
}

ExprResult Invocation::EvaluateBuiltinAtomic(const sem::BuiltinFn* builtin,
                                             const ast::CallExpression* call) {
    auto* ptr = GetResult(call->args[0]).Pointer();

    switch (builtin->Fn()) {
        case wgsl::BuiltinFn::kAtomicCompareExchangeWeak: {
            bool exchanged;
            auto* cmp = GetResult(call->args[1]).Value();
            auto* value = GetResult(call->args[2]).Value();
            auto* old_value = ptr->AtomicCompareExchange(cmp, value, exchanged);

            // Build a struct result that contains {old_value, exchanged}.
            auto* exchanged_constant = executor_.Builder().constants.Get(exchanged);
            auto result = executor_.ConstEval().ArrayOrStructCtor(
                builtin->ReturnType(), tint::Vector{old_value, exchanged_constant});
            return ExprResult::MakeValue(result.Get());
        }
        case wgsl::BuiltinFn::kAtomicLoad: {
            return ExprResult::MakeValue(ptr->AtomicLoad());
        }
        case wgsl::BuiltinFn::kAtomicStore: {
            auto* value = GetResult(call->args[1]).Value();
            ptr->AtomicStore(value);
            return ExprResult();
        }
        case wgsl::BuiltinFn::kAtomicAdd: {
            auto* value = GetResult(call->args[1]).Value();
            return ExprResult::MakeValue(ptr->AtomicRMW(AtomicOp::kAdd, value));
        }
        case wgsl::BuiltinFn::kAtomicSub: {
            auto* value = GetResult(call->args[1]).Value();
            return ExprResult::MakeValue(ptr->AtomicRMW(AtomicOp::kSub, value));
        }
        case wgsl::BuiltinFn::kAtomicMax: {
            auto* value = GetResult(call->args[1]).Value();
            return ExprResult::MakeValue(ptr->AtomicRMW(AtomicOp::kMax, value));
        }
        case wgsl::BuiltinFn::kAtomicMin: {
            auto* value = GetResult(call->args[1]).Value();
            return ExprResult::MakeValue(ptr->AtomicRMW(AtomicOp::kMin, value));
        }
        case wgsl::BuiltinFn::kAtomicAnd: {
            auto* value = GetResult(call->args[1]).Value();
            return ExprResult::MakeValue(ptr->AtomicRMW(AtomicOp::kAnd, value));
        }
        case wgsl::BuiltinFn::kAtomicOr: {
            auto* value = GetResult(call->args[1]).Value();
            return ExprResult::MakeValue(ptr->AtomicRMW(AtomicOp::kOr, value));
        }
        case wgsl::BuiltinFn::kAtomicXor: {
            auto* value = GetResult(call->args[1]).Value();
            return ExprResult::MakeValue(ptr->AtomicRMW(AtomicOp::kXor, value));
        }
        case wgsl::BuiltinFn::kAtomicExchange: {
            auto* value = GetResult(call->args[1]).Value();
            return ExprResult::MakeValue(ptr->AtomicRMW(AtomicOp::kXchg, value));
        }
        default:
            executor_.ReportFatalError("unhandled atomic builtin call", call->source);
            return ExprResult();
    }
}

Invocation::ExprEvaluator Invocation::EnqueueIdentifier(const ast::IdentifierExpression* ident) {
    return [&, ident]() {
        auto* var = executor_.Sem().GetVal(ident)->UnwrapLoad()->As<sem::VariableUser>();
        if (var->Stage() == core::EvaluationStage::kOverride) {
            // Get the value of a named pipeline-override from the shader executor.
            auto* value = executor_.GetNamedOverride(var->Variable());
            if (value == nullptr) {
                executor_.ReportFatalError("missing named pipeline-override value", ident->source);
                return ExprResult();
            }
            return ExprResult::MakeValue(value);
        } else if (!variable_values_.count(var->Variable())) {
            executor_.ReportFatalError("missing variable value", ident->source);
            return ExprResult();
        }
        return variable_values_.at(var->Variable());
    };
}

Invocation::ExprEvaluator Invocation::EnqueueIndexAccessor(
    const ast::IndexAccessorExpression* accessor) {
    EnqueueExpression(accessor->object);
    EnqueueExpression(accessor->index);
    return [&, accessor]() {
        auto obj = GetResult(accessor->object);
        auto idx = GetResult(accessor->index).Value()->ValueAs<uint32_t>();
        auto* obj_ty = executor_.Sem().GetVal(accessor->object)->Type()->UnwrapRef();
        auto* elem_ty = executor_.Sem().Get(accessor)->Type()->UnwrapRef();
        uint32_t stride = elem_ty->Size();
        if (auto* arr = obj_ty->As<core::type::Array>()) {
            stride = arr->Stride();
        } else if (auto* mat = obj_ty->As<core::type::Matrix>()) {
            stride = mat->ColumnStride();
        }
        switch (obj.Kind()) {
            case ExprResult::Kind::kPointer:
            case ExprResult::Kind::kReference:
                return ExprResult::MakeReference(obj.MemoryView()->CreateSubview(
                    elem_ty, idx * stride, elem_ty->Size(), accessor->source));
            case ExprResult::Kind::kValue:
                if (idx * stride >= obj.Value()->Type()->Size()) {
                    diag::List list;
                    list.AddWarning(diag::System::Interpreter, accessor->source)
                        << "index " << idx << " is out of bounds";
                    executor_.ReportError(std::move(list));
                    return ExprResult::MakeValue(
                        executor_.ConstEval().Zero(elem_ty, tint::Empty, {}).Get());
                }
                return ExprResult::MakeValue(obj.Value()->Index(idx));
            default:
                executor_.ReportFatalError("unhandled index accessor object kind",
                                           accessor->source);
                return ExprResult();
        }
    };
}

Invocation::ExprEvaluator Invocation::EnqueueMemberAccessor(
    const ast::MemberAccessorExpression* accessor) {
    EnqueueExpression(accessor->object);
    return [&, accessor]() {
        auto obj = GetResult(accessor->object);
        auto* result_ty = executor_.Sem().Get(accessor)->Type()->UnwrapRef();
        auto* sem_accessor = executor_.Sem().Get(accessor)->UnwrapLoad();
        return Switch(
            sem_accessor,  //
            [&](const sem::StructMemberAccess* member_access) {
                switch (obj.Kind()) {
                    case ExprResult::Kind::kPointer:
                    case ExprResult::Kind::kReference: {
                        auto* view = obj.MemoryView();
                        uint64_t size = result_ty->Size();
                        uint32_t offset = member_access->Member()->Offset();

                        // If the member is a runtime-sized array, expand the size of the returned
                        // view to consume the remainder of the object.
                        if (auto* arr = result_ty->UnwrapRef()->As<core::type::Array>();
                            arr && arr->Count()->Is<core::type::RuntimeArrayCount>()) {
                            size = view->Size() - offset;
                        }

                        return ExprResult::MakeReference(
                            view->CreateSubview(result_ty, offset, size, accessor->source));
                    }
                    case ExprResult::Kind::kValue:
                        return ExprResult::MakeValue(
                            obj.Value()->Index(member_access->Member()->Index()));
                    default:
                        executor_.ReportFatalError("unhandled member accessor object kind",
                                                   accessor->source);
                        return ExprResult();
                }
            },
            [&](const sem::Swizzle* swizzle) {
                auto& indices = swizzle->Indices();
                switch (obj.Kind()) {
                    case ExprResult::Kind::kPointer:
                    case ExprResult::Kind::kReference:
                        if (indices.Length() == 1) {
                            return ExprResult::MakeReference(obj.MemoryView()->CreateSubview(
                                result_ty, indices[0] * result_ty->Size(), result_ty->Size(),
                                accessor->source));
                        }
                        // WGSL does not support creating references to multi-component swizzles, so
                        // assume that we can just create a value result instead.
                        [[fallthrough]];
                    case ExprResult::Kind::kValue: {
                        auto* value = obj.Value();

                        // Special case for single-element swizzles, which return a scalar.
                        if (indices.Length() == 1) {
                            return ExprResult::MakeValue(value->Index(indices[0]));
                        }

                        tint::Vector<const core::constant::Value*, 4> elements;
                        for (uint32_t i = 0; i < indices.Length(); i++) {
                            elements.Push(value->Index(indices[i]));
                        }
                        return ExprResult::MakeValue(
                            executor_.ConstEval().VecInitS(result_ty, elements, {}).Get());
                    }
                    default:
                        executor_.ReportFatalError("unhandled swizzle object kind",
                                                   accessor->source);
                        return ExprResult();
                }
            },
            [&](Default) -> ExprResult {
                executor_.ReportFatalError("unhandled member accessor expression",
                                           accessor->source);
                return ExprResult();
            });
    };
}

Invocation::ExprEvaluator Invocation::EnqueueUnaryOp(const ast::UnaryOpExpression* unary) {
    EnqueueExpression(unary->expr);
    return [&, unary]() {
        auto expr = GetResult(unary->expr);
        switch (unary->op) {
            case core::UnaryOp::kAddressOf: {
                return ExprResult::MakePointer(expr.Reference());
            }
            case core::UnaryOp::kIndirection: {
                return ExprResult::MakeReference(expr.Pointer());
            }
            default: {
                // Look for a const-eval implementation of this unary operator.
                auto args = tint::Vector{expr.Value()};
                auto op = executor_.IntrinsicTable().Lookup(unary->op, args[0]->Type(),
                                                            core::EvaluationStage::kConstant);
                TINT_ASSERT(op == Success);
                auto result = (executor_.ConstEval().*op->const_eval_fn)(op->return_type, args,
                                                                         unary->source);
                if (result != Success) {
                    executor_.ReportFatalError("unary expression evaluation failed", unary->source);
                    return ExprResult();
                }
                return ExprResult::MakeValue(result.Get());
            }
        }
    };
}

Invocation::StmtExecutor Invocation::Assignment(const ast::AssignmentStatement* assign) {
    EnqueueExpression(assign->lhs);
    EnqueueExpression(assign->rhs);
    return [&, assign]() {
        auto rhs = GetResult(assign->rhs);
        if (!assign->lhs->Is<ast::PhonyExpression>()) {
            auto lhs = GetResult(assign->lhs).Reference();
            lhs->Store(rhs.Value());
        }

        NextStatement();
    };
}

Invocation::StmtExecutor Invocation::Break(const ast::BreakStatement*) {
    return [&]() { EndBlock(BlockEndKind::kBreak); };
}

Invocation::StmtExecutor Invocation::BreakIf(const ast::BreakIfStatement* brk) {
    EnqueueExpression(brk->condition);
    return [&, brk]() {
        if (GetResult(brk->condition).Value()->ValueAs<bool>()) {
            EndBlock(BlockEndKind::kBreak);
        } else {
            NextStatement();
        }
    };
}

Invocation::StmtExecutor Invocation::CallStmt(const ast::CallStatement* call) {
    EnqueueExpression(call->expr);
    return [&]() { NextStatement(); };
}

Invocation::StmtExecutor Invocation::CompoundAssignment(
    const ast::CompoundAssignmentStatement* assign) {
    EnqueueExpression(assign->lhs);
    EnqueueExpression(assign->rhs);
    return [&, assign]() {
        auto lhs = GetResult(assign->lhs).Reference();
        auto rhs = GetResult(assign->rhs).Value();

        // Evaluate the binary operator for (lhs OP rhs).
        auto args = tint::Vector{lhs->Load(), rhs};
        auto op = executor_.IntrinsicTable().Lookup(assign->op, args[0]->Type(), args[1]->Type(),
                                                    core::EvaluationStage::kConstant, true);
        TINT_ASSERT(op == Success);
        auto result =
            (executor_.ConstEval().*op->const_eval_fn)(op->return_type, args, assign->source);
        if (result != Success) {
            executor_.ReportFatalError("binary expression evaluation failed", assign->source);
            return;
        }

        // Store the result back to the LHS.
        lhs->Store(result.Get());

        NextStatement();
    };
}

Invocation::StmtExecutor Invocation::Continue(const ast::ContinueStatement*) {
    return [&]() { EndBlock(BlockEndKind::kContinue); };
}

Invocation::StmtExecutor Invocation::If(const ast::IfStatement* if_stmt) {
    EnqueueExpression(if_stmt->condition);
    return [&, if_stmt]() {
        auto condition = GetResult(if_stmt->condition).Value()->ValueAs<bool>();
        if (condition) {
            StartBlock(if_stmt->body);
        } else {
            if (if_stmt->else_statement) {
                auto& current_block = call_stack_.back()->block_stack.top();
                auto* if_stmt_itr = current_block.current_statement;

                // Switch to the else statement.
                current_block.current_statement = &if_stmt->else_statement;
                auto exec_else = PrepareStatement(if_stmt->else_statement);

                // Just before executing the else statement, set the current statement back to the
                // original if statement. This is so that when we return from executing the else
                // statement (which is always a block or an 'else if' which will enter a block), we
                // move onto the next statement after the original if.
                current_block.current_stmt_exec = [&, exec_else, if_stmt_itr]() {
                    current_block.current_statement = if_stmt_itr;
                    exec_else();
                };
            } else {
                NextStatement();
            }
        }
    };
}

Invocation::StmtExecutor Invocation::ForLoop(const ast::ForLoopStatement* loop) {
    if (loop->initializer) {
        return [&, loop]() {
            // Switch to the initializer statement.
            auto& current_block = call_stack_.back()->block_stack.top();
            auto* loop_stmt_itr = current_block.current_statement;
            // TODO(jrprice): We need to create an additional scope for the for loop that includes
            // the initializer expression.
            current_block.current_statement = &loop->initializer;
            auto exec_initializer = PrepareStatement(loop->initializer);

            // Execute the initializer statement and then switch back to the main loop statement to
            // evaluate the condition.
            current_block.current_stmt_exec = [&, loop, exec_initializer, loop_stmt_itr]() {
                exec_initializer();
                current_block.current_statement = loop_stmt_itr;
                current_block.current_stmt_exec = LoopCondition(loop->condition, loop->body);
            };
        };
    } else {
        // No initializer statement, so go straight to the condition.
        return LoopCondition(loop->condition, loop->body);
    }
}

Invocation::StmtExecutor Invocation::IncrementDecrement(
    const ast::IncrementDecrementStatement* inc_dec) {
    EnqueueExpression(inc_dec->lhs);
    return [&, inc_dec]() {
        auto lhs = GetResult(inc_dec->lhs).Reference();
        auto* type = lhs->Type();

        // Create a constant representing integer 1.
        auto* one = executor_.Builder().constants.Get(core::AInt(1));

        // Evaluate (lhs + 1) or (lhs - 1).
        tint::core::constant::Eval::Result result;
        auto args = tint::Vector{lhs->Load(), one};
        if (inc_dec->increment) {
            result = executor_.ConstEval().Plus(type, args, inc_dec->source);
        } else {
            result = executor_.ConstEval().Minus(type, args, inc_dec->source);
        }

        // Store the result back to the LHS.
        lhs->Store(result.Get());

        NextStatement();
    };
}

Invocation::StmtExecutor Invocation::Loop(const ast::LoopStatement* loop) {
    return [&, loop]() { StartBlock(loop->body); };
}

Invocation::StmtExecutor Invocation::ReturnStmt(const ast::ReturnStatement* ret) {
    EnqueueExpression(ret->value);
    return [&, ret]() {
        ExprResult ret_val;
        if (ret->value) {
            ret_val = ExprResult::MakeValue(GetResult(ret->value).Value());
        }
        ReturnFromFunction(ret_val);
    };
}

Invocation::StmtExecutor Invocation::SwitchStmt(const ast::SwitchStatement* swtch) {
    EnqueueExpression(swtch->condition);
    return [&, swtch]() {
        auto condition = GetResult(swtch->condition).Value()->ValueAs<uint32_t>();

        // Find the case selector that matches the condition.
        const ast::CaseStatement* default_case = nullptr;
        const ast::CaseStatement* selected_case = nullptr;
        for (auto* c : swtch->body) {
            for (auto* s : c->selectors) {
                if (s->IsDefault()) {
                    // Note the default case, as we will use it if no matching selector is found.
                    default_case = c;
                } else {
                    auto* value = executor_.Sem().GetVal(s->expr)->ConstantValue();
                    if (value->ValueAs<uint32_t>() == condition) {
                        // The selector matches, so use this case.
                        selected_case = c;
                        break;
                    }
                }
            }
            if (selected_case) {
                break;
            }
        }
        if (!selected_case) {
            // No selector matched, so use the default case instead.
            TINT_ASSERT(default_case != nullptr);
            selected_case = default_case;
        }

        StartBlock(selected_case->body);
    };
}

Invocation::StmtExecutor Invocation::VariableDecl(const ast::VariableDeclStatement* decl) {
    TINT_ASSERT(!call_stack_.empty());

    EnqueueExpression(decl->variable->initializer);
    return [&, decl]() {
        auto* sem_var = executor_.Sem().Get(decl->variable);
        auto* store_type = sem_var->Type()->UnwrapRef();

        // Evaluate the initializer expression.
        ExprResult init_value;
        if (decl->variable->initializer) {
            init_value = GetResult(decl->variable->initializer);
        } else {
            // Generate a zero-init value when no initializer is provided.
            auto zero = executor_.ConstEval().Zero(store_type, {}, {});
            if (zero != Success) {
                executor_.ReportFatalError("zero initializer generation failed", decl->source);
                return;
            }
            init_value = ExprResult::MakeValue(zero.Get());
        }

        Switch(
            decl->variable,             //
            [&](const ast::Const*) {},  // No-op: all uses will use the constant value directly.
            [&](const ast::Let*) { variable_values_[sem_var] = init_value; },
            [&](const ast::Var*) {
                // Create a memory allocation and a view into it.
                auto alloc = std::make_unique<Memory>(store_type->Size());
                auto view = alloc->CreateView(&executor_, sem_var->AddressSpace(), store_type,
                                              decl->variable->source);
                call_stack_.back()->block_stack.top().allocations.push_back(std::move(alloc));

                // Store the value of the initializer.
                view->Store(init_value.Value());

                variable_values_[sem_var] = ExprResult::MakeReference(view);
            },
            [&](Default) {
                executor_.ReportFatalError("unhandled variable declaration type",
                                           decl->variable->source);
                return;
            });

        // Register the variable's identifier in the current scope.
        auto ident = decl->variable->name->symbol.Name();
        call_stack_.back()->identifiers.Set(ident, sem_var);

        NextStatement();
    };
}

Invocation::StmtExecutor Invocation::While(const ast::WhileStatement* loop) {
    return LoopCondition(loop->condition, loop->body);
}

Invocation::StmtExecutor Invocation::LoopCondition(const ast::Expression* condition,
                                                   const ast::BlockStatement* body) {
    EnqueueExpression(condition);
    return [&, condition, body]() {
        if (!condition || GetResult(condition).Value()->ValueAs<bool>()) {
            StartBlock(body);
        } else {
            NextStatement();
        }
    };
}

void Invocation::NextStatement() {
    TINT_ASSERT(!call_stack_.empty());
    TINT_ASSERT(!call_stack_.back()->block_stack.empty());
    auto& current_block = call_stack_.back()->block_stack.top();

    // Move to the next statement and prepare to execute it.
    auto* stmt = ++current_block.current_statement;
    if (stmt != current_block.block->statements.end()) {
        current_block.current_stmt_exec = PrepareStatement(*stmt);
    }
}

const core::constant::Value* Invocation::EvaluateOverrideExpression(const ast::Expression* expr) {
    if (executor_.Sem().GetVal(expr)->Stage() > core::EvaluationStage::kOverride) {
        executor_.ReportFatalError("attemping to evaluate non-override expression", expr->source);
        return nullptr;
    }

    // Push a fake call stack and block entry.
    call_stack_.push_back(std::make_unique<CallStackEntry>(nullptr));
    call_stack_.back()->block_stack.push({nullptr, nullptr});
    auto& block = call_stack_.back()->block_stack.top();

    // Enqueue the target expression.
    EnqueueExpression(expr);

    // Evaluate everything in the expression queue.
    while (block.next_expr < block.expr_queue.size()) {
        auto& next = block.expr_queue[block.next_expr];
        auto result = next.func();
        block.expr_results[next.expr] = result;
        block.next_expr++;
    }

    // Get the final result.
    auto* result = GetResult(expr).Value();
    call_stack_.pop_back();
    return result;
}

}  // namespace tint::interp
