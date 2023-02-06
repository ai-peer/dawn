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

#ifndef SRC_TINT_INTERP_INVOCATION_H_
#define SRC_TINT_INTERP_INVOCATION_H_

#include <functional>
#include <stack>
#include <unordered_map>
#include <vector>

#include "tint/interp/uvec3.h"
#include "tint/scope_stack.h"

// Forward Declarations
namespace tint::ast {
class AssignmentStatement;
class BinaryExpression;
class BitcastExpression;
class BlockStatement;
class BreakStatement;
class BreakIfStatement;
class CallExpression;
class CallStatement;
class CompoundAssignmentStatement;
class ContinueStatement;
class IdentifierExpression;
class IfStatement;
class IncrementDecrementStatement;
class IndexAccessorExpression;
class Expression;
class ForLoopStatement;
class Function;
class LoopStatement;
class MemberAccessorExpression;
class ReturnStatement;
class Statement;
class SwitchStatement;
class UnaryOpExpression;
class VariableDeclStatement;
class WhileStatement;
}  // namespace tint::ast
namespace tint::constant {
class Value;
}  // namespace tint::constant
namespace tint::sem {
class Builtin;
class Variable;
}  // namespace tint::sem
namespace tint::interp {
class Memory;
class MemoryView;
class ShaderExecutor;
}  // namespace tint::interp

namespace tint::interp {

/// An `ExprResult` object represents the result of evaluating an expression. It could be a value
/// (constant::Value), or a reference/pointer (MemoryView).
class ExprResult {
  public:
    /// The type of a ExprResult object.
    enum class Kind { kInvalid, kValue, kReference, kPointer };

    /// Default constructor.
    ExprResult() { kind_ = Kind::kInvalid; }

    /// Create an ExprResult that represents a value.
    static ExprResult MakeValue(const constant::Value* value) {
        TINT_ASSERT(Interpreter, value != nullptr);
        ExprResult result;
        result.kind_ = Kind::kValue;
        result.value_ = value;
        return result;
    }

    /// Create an ExprResult that represents a reference.
    static ExprResult MakeReference(MemoryView* view) {
        TINT_ASSERT(Interpreter, view != nullptr);
        ExprResult result;
        result.kind_ = Kind::kReference;
        result.memory_view_ = view;
        return result;
    }

    /// Create an ExprResult that represents a pointer.
    static ExprResult MakePointer(MemoryView* view) {
        TINT_ASSERT(Interpreter, view != nullptr);
        ExprResult result;
        result.kind_ = Kind::kPointer;
        result.memory_view_ = view;
        return result;
    }

    /// @returns the type of this ExprResult object
    Kind Kind() const { return kind_; }

    /// @returns the value of this object
    const constant::Value* Value() {
        TINT_ASSERT(Interpreter, kind_ == Kind::kValue);
        return value_;
    }

    /// @returns the memory view associated with this object
    MemoryView* Reference() {
        TINT_ASSERT(Interpreter, kind_ == Kind::kReference);
        return memory_view_;
    }

    /// @returns the memory view associated with this object
    MemoryView* Pointer() {
        TINT_ASSERT(Interpreter, kind_ == Kind::kPointer);
        return memory_view_;
    }

  private:
    enum Kind kind_;
    union {
        const constant::Value* value_ = nullptr;
        MemoryView* memory_view_;
    };
};

class Invocation {
  private:
    // A function used to evaluate an expression and returns its result.
    using ExprEvaluator = std::function<ExprResult()>;
    // A function used to execute an statement.
    using StmtExecutor = std::function<void()>;

    /// An entry in the block stack to track the current position of the invocation.
    struct BlockStackEntry {
        const ast::BlockStatement* block;
        const ast::Statement* const* current_statement;
        std::vector<std::unique_ptr<Memory>> allocations{};

        /// An entry in the expression evaluation queue.
        struct ExprQueueEntry {
            const ast::Expression* expr;
            ExprEvaluator func;
        };
        uint32_t next_expr = 0;                    // Index of the next expression in the queue.
        std::vector<ExprQueueEntry> expr_queue{};  // Expression evaluation queue.
        StmtExecutor current_stmt_exec{};          // Current statement executor.

        // Map from RHS index to short-circuiting operator index.
        std::unordered_map<uint32_t, uint32_t> short_circuiting_ops{};

        // Evaluated expression results for the current statement.
        std::unordered_map<const ast::Expression*, ExprResult> expr_results{};
    };

  public:
    /// The possible states for an invocation.
    enum State {
        kReady,     // Invocation can be stepped.
        kBarrier,   // Invocation is waiting at a barrier.
        kFinished,  // Invocation has finished execution.
    };

    /// An entry in the function call stack to track the current position of the invocation.
    struct CallStackEntry {
        CallStackEntry(const ast::Function* f) : func(f) {}
        const ast::Function* func;
        ScopeStack<std::string, const sem::Variable*> identifiers{};

      private:
        std::stack<BlockStackEntry> block_stack{};
        friend Invocation;
    };
    using CallStack = std::vector<std::unique_ptr<CallStackEntry>>;

    /// Constructor for an invocation that is executing a shader.
    Invocation(ShaderExecutor& executor,
               UVec3 group_id,
               UVec3 local_id,
               const std::unordered_map<const sem::Variable*, MemoryView*>& allocations = {});

    /// Constructor for a helper invocation that will be used to evaluate isolated expressions.
    Invocation(ShaderExecutor& executor);

    /// Destructor
    ~Invocation();

    /// @returns the current state of the invocation
    State GetState() const;

    /// Step the invocation.
    void Step();

    /// @returns the barrier that this invocation is waiting at, or nullptr
    const ast::CallExpression* Barrier() const { return barrier_; }

    /// Notify the invocation that it may proceed past the barrier that it is waiting at.
    void ClearBarrier();

    /// @param frame the optional frame index
    /// @returns the current block
    const ast::BlockStatement* CurrentBlock(uint32_t frame = 0) const;

    /// @param frame the optional frame index
    /// @returns the current statement
    const ast::Statement* CurrentStatement(uint32_t frame = 0) const;

    /// @param frame the optional frame index
    /// @returns the current expression
    const ast::Expression* CurrentExpression(uint32_t frame = 0) const;

    /// @returns the function call stack
    const CallStack& GetCallStack() const { return call_stack_; }

    /// @returns the stringified value of `identifier`, or an error string
    std::string GetValue(std::string identifier) const;

    /// @returns the local id for this invocation
    const UVec3& LocalInvocationId() const { return local_invocation_id_; }

    /// @returns the local index for this invocation
    uint32_t LocalInvocationIndex() const { return local_invocation_index_; }

    /// @returns the workgroup id for this invocation
    const UVec3& WorkgroupId() const { return workgroup_id_; }

    /// Evaluate an AST expression in isolation (outside the context of a shader).
    /// The expression must be an override expression.
    /// @param expr the expression to evaluate
    /// @returns the evaluated result, or nullptr if evaluation failed
    const constant::Value* EvaluateOverrideExpression(const ast::Expression* expr);

  private:
    ShaderExecutor& executor_;
    const UVec3 workgroup_id_{};
    const UVec3 local_invocation_id_{};
    const uint32_t local_invocation_index_ = 0;

    const ast::CallExpression* barrier_ = nullptr;

    CallStack call_stack_;
    std::unordered_map<const sem::Variable*, ExprResult> variable_values_;
    std::vector<std::unique_ptr<Memory>> private_allocations_;
    ScopeStack<std::string, const sem::Variable*> global_identifiers_;

    enum class BlockEndKind {
        kRegular,
        kBreak,
        kContinue,
    };

    /// Updates the invocation to start executing `func`.
    void StartFunction(const ast::Function* func, utils::VectorRef<ExprResult> args = {});
    /// Updates the invocation to start executing `block`.
    void StartBlock(const ast::BlockStatement* block);
    /// Updates the invocation to move on from executing the current block, possibly breaking out
    /// from a loop/switch statement.
    void EndBlock(BlockEndKind kind = BlockEndKind::kRegular);
    /// Advance to the next statement.
    void NextStatement();
    /// Return from the current function.
    void ReturnFromFunction(ExprResult return_value = {});

    /// Prepare the evaluation of a statement.
    /// @param stmt the statement to be evaluated
    /// @returns a function used to execute the statement once its dependencies are met
    StmtExecutor PrepareStatement(const ast::Statement* stmt);

    /// Enqueue the evaluation of an expression.
    /// @param expr the expression to be evaluated
    void EnqueueExpression(const ast::Expression* expr);

    /// Gets the evaluated result for an expression.
    /// @param expr the expression to retrieve the result for
    /// @returns the evaluated result
    ExprResult GetResult(const ast::Expression* expr);

    // Expression handlers.
    // These enqueue any child expressions and return a function that can be used to evaluate the
    // expression once all of its dependencies have been met.
    ExprEvaluator EnqueueBinary(const ast::BinaryExpression* binary);
    ExprEvaluator EnqueueBitcast(const ast::BitcastExpression* bitcast);
    ExprEvaluator EnqueueCall(const ast::CallExpression* call);
    ExprEvaluator EnqueueIdentifier(const ast::IdentifierExpression* ident);
    ExprEvaluator EnqueueIndexAccessor(const ast::IndexAccessorExpression* accessor);
    ExprEvaluator EnqueueMemberAccessor(const ast::MemberAccessorExpression* accessor);
    ExprEvaluator EnqueueUnaryOp(const ast::UnaryOpExpression* unary);

    // Statement handlers.
    // These enqueue any required expressions and return a function that can be used to execute the
    // statement once all of its dependencies have been met.
    StmtExecutor Assignment(const ast::AssignmentStatement* assign);
    StmtExecutor Break(const ast::BreakStatement* brk);
    StmtExecutor BreakIf(const ast::BreakIfStatement* brk);
    StmtExecutor CallStmt(const ast::CallStatement* call);
    StmtExecutor CompoundAssignment(const ast::CompoundAssignmentStatement* assign);
    StmtExecutor Continue(const ast::ContinueStatement* cont);
    StmtExecutor ForLoop(const ast::ForLoopStatement* loop);
    StmtExecutor If(const ast::IfStatement* if_stmt);
    StmtExecutor IncrementDecrement(const ast::IncrementDecrementStatement* inc_dec);
    StmtExecutor Loop(const ast::LoopStatement* loop);
    StmtExecutor ReturnStmt(const ast::ReturnStatement* ret);
    StmtExecutor SwitchStmt(const ast::SwitchStatement* swtch);
    StmtExecutor VariableDecl(const ast::VariableDeclStatement* decl);
    StmtExecutor While(const ast::WhileStatement* loop);

    // Helper that makes a statement executor that will evaluate the loop condition and move to the
    // body if it is true, otherwise it will move past the loop.
    StmtExecutor LoopCondition(const ast::Expression* condition, const ast::BlockStatement* body);

    // Helpers for evaluating builtin functions.
    ExprResult EvaluateBuiltin(const sem::Builtin* builtin, const ast::CallExpression* call);
    ExprResult EvaluateBuiltinAtomic(const sem::Builtin* builtin, const ast::CallExpression* call);
};

}  // namespace tint::interp

#endif  // SRC_TINT_INTERP_INVOCATION_H_
