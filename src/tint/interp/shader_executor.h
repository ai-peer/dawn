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

#ifndef SRC_TINT_INTERP_SHADER_EXECUTOR_H_
#define SRC_TINT_INTERP_SHADER_EXECUTOR_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/tint/lang/core/intrinsic/table.h"
#include "src/tint/lang/wgsl/intrinsic/dialect.h"
#include "tint/interp/memory.h"
#include "tint/interp/uvec3.h"
#include "tint/lang/core/constant/eval.h"
#include "tint/lang/wgsl/program/program_builder.h"
#include "tint/utils/diagnostic/source.h"
#include "tint/utils/memory/block_allocator.h"
#include "tint/utils/result/result.h"

// Forward Declarations
namespace tint {
class StyledTextPrinter;
}  // namespace tint
namespace tint::ast {
class CallExpression;
class Function;
}  // namespace tint::ast
namespace tint::core::Intrinsic {
class Table;
}  // namespace tint::core::Intrinsic
namespace tint::interp {
class Invocation;
class Workgroup;
}  // namespace tint::interp
namespace tint::sem {
class Info;
}  // namespace tint::sem

namespace tint::interp {

/// Binding represents a single resource that is being bound to a variable in the shader.
struct Binding {
    /// The buffer being bound.
    Memory* buffer = nullptr;
    /// The offset of the buffer in bytes.
    uint64_t buffer_offset = 0;
    /// The sie of the buffer in bytes.
    uint64_t buffer_size = 0;

    /// Constructor - make an invalid binding.
    Binding() = default;

    /// Create a buffer binding.
    /// @param buffer the memory allocation backing this buffer
    /// @param offset the offset from the start of the buffer in bytes
    /// @param size the size of binding in bytes
    /// @returns the Binding object
    static Binding MakeBufferBinding(Memory* buffer, uint64_t offset, uint64_t size) {
        Binding b;
        b.buffer = buffer;
        b.buffer_offset = offset;
        b.buffer_size = size;
        return b;
    }
};
/// BindingList is a map from binding point (group, index) to the resource bound to it.
using BindingList = std::unordered_map<BindingPoint, Binding>;

/// NamedOverrideList is a map from named pipeline-override to the value being overridden for it.
using NamedOverrideList = std::unordered_map<std::string, double>;

/// A ShaderExecutor handles the execution of a shader. It also allows callbacks to be registered
/// to receive information about events that occur during execution, which can be used to implement
/// dynamic analysis tools that use the interpreter.
class ShaderExecutor {
  public:
    /// The result of a method which may fail with a reason string.
    using Result = tint::Result<tint::SuccessType, std::string>;

    /// Create a shader executor.
    /// @param program the program object
    /// @param entry_point the entry point to run
    /// @param overrides the named pipeline overrides to use
    /// @returns the created shader executor object, or a failure reason
    static tint::Result<std::unique_ptr<ShaderExecutor>, std::string>
    Create(const Program& program, std::string entry_point, NamedOverrideList&& overrides);

    /// Destructor
    ~ShaderExecutor();

    /// Run the shader.
    /// @param workgroup_count the number of workgroups to dispatch
    /// @param bindings the resource bindings to use
    /// @returns `Success` or a failure reason if a fatal error occurs during execution
    Result Run(UVec3 workgroup_count, BindingList&& bindings);

    /// @returns the currently executing invocation, or nullptr
    const Invocation* CurrentInvocation() const;

    /// @returns the currently executing workgroup, or nullptr
    Workgroup* CurrentWorkgroup() const { return current_group_.get(); }

    /// Switch the workgroup that is currently executing.
    /// @param group_id the workgroup ID of the workgroup to switch to
    /// @returns true on success, false if the ID is invalid or the group has already finished
    bool SelectWorkgroup(UVec3 group_id);

    /// @returns the entry point for this execution
    const ast::Function* EntryPoint() const { return entry_point_; }

    /// @returns the program object
    const tint::Program& Program() { return program_; }

    /// @returns the ProgramBuilder object to use for creating temporary AST nodes
    ProgramBuilder& Builder() { return builder_; }

    /// @returns the ConstEval object to use for expression evaluation
    core::constant::Eval& ConstEval() { return *const_eval_; }

    /// @returns the IntrinsicTable object to use for expression evaluation
    core::intrinsic::Table<wgsl::intrinsic::Dialect>& IntrinsicTable() { return *intrinsic_table_; }

    /// @returns the semantic info used for this execution
    const sem::Info& Sem() const { return builder_.Sem(); }

    /// @returns the source file that corresponds to the program being executed
    const Source::File& SourceFile() const;

    /// @returns the memory view allocator used for this execution
    tint::BlockAllocator<MemoryView>& MemoryViews() { return memory_views_; }

    /// @returns the workgroup count for this execution
    const UVec3& WorkgroupCount() const { return workgroup_count_; }

    /// @returns the workgroup size for this execution
    const UVec3& WorkgroupSize() const { return workgroup_size_; }

    /// @returns the set of bindings used for this execution
    const std::unordered_map<const sem::GlobalVariable*, MemoryView*>& Bindings() const {
        return bindings_;
    }

    /// Get the value of a named override declaration.
    /// @param named_override the named override declaration
    /// @returns the value of that declaration in this execution, or nullptr
    const core::constant::Value* GetNamedOverride(const sem::Variable* named_override) const {
        auto itr = named_overrides_.find(named_override);
        if (itr == named_overrides_.end()) {
            return nullptr;
        }
        return itr->second;
    }

    /// Flush any errors that have been captured in the program builder diagnostics list.
    void FlushErrors();

    /// Report a fatal error that should halt execution.
    /// @param msg the error message
    /// @param source the optional source location that caused the error
    void ReportFatalError(std::string msg, Source source = {});

    ////////////////////////////////////////////////////////////////
    // Event callbacks
    ////////////////////////////////////////////////////////////////

    /// A callback for barrier events.
    using BarrierCallback = std::function<void(const Workgroup*, const ast::CallExpression*)>;
    /// A callback for dispatch begin events.
    using DispatchBeginCallback = std::function<void()>;
    /// A callback for dispatch complete events.
    using DispatchCompleteCallback = std::function<void()>;
    /// A callback for error events.
    using ErrorCallback = std::function<void(diag::List&& error)>;
    /// A callback for memory load events.
    using MemoryLoadCallback = std::function<void(const MemoryView*)>;
    /// A callback for memory store events.
    using MemoryStoreCallback = std::function<void(const MemoryView*)>;
    /// A callback for post-step events.
    using PostStepCallback = std::function<void(const Invocation*)>;
    /// A callback for pre-step events.
    using PreStepCallback = std::function<void(const Invocation*)>;
    /// A callback for workgroup begin events.
    using WorkgroupBeginCallback = std::function<void(const Workgroup*)>;
    /// A callback for workgroup complete events.
    using WorkgroupCompleteCallback = std::function<void(const Workgroup*)>;

    /// Add a callback for barrier events.
    /// @param callback the callback
    void AddBarrierCallback(BarrierCallback callback) { barrier_callbacks_.push_back(callback); }

    /// Report a barrier event.
    /// @param workgroup the workgroup performing the barrier
    /// @param call the barrier builtin call expression
    void ReportBarrier(const Workgroup* workgroup, const ast::CallExpression* call);

    /// Add a callback for dispatch begin events.
    /// @param callback the callback
    void AddDispatchBeginCallback(DispatchBeginCallback callback) {
        dispatch_begin_callbacks_.push_back(callback);
    }

    /// Report a dispatch begin event.
    void ReportDispatchBegin();

    /// Add a callback for dispatch complete events.
    /// @param callback the callback
    void AddDispatchCompleteCallback(DispatchCompleteCallback callback) {
        dispatch_complete_callbacks_.push_back(callback);
    }

    /// Report a dispatch complete event.
    void ReportDispatchComplete();

    /// Add a callback for errors.
    /// @param callback the callback
    void AddErrorCallback(ErrorCallback callback) { error_callbacks_.push_back(callback); }

    /// Report a non-fatal diagnostic error that should be displayed to the user.
    /// @param error the diagnostic message(s)
    void ReportError(diag::List&& error);

    /// Add a callback for memory load events.
    /// @param callback the callback
    void AddMemoryLoadCallback(MemoryLoadCallback callback) {
        memory_load_callbacks_.push_back(callback);
    }

    /// Report a memory load event.
    /// @param view the memory view being loaded from
    void ReportMemoryLoad(const MemoryView* view);

    /// Add a callback for memory store events.
    /// @param callback the callback
    void AddMemoryStoreCallback(MemoryStoreCallback callback) {
        memory_store_callbacks_.push_back(callback);
    }

    /// Report a memory store event.
    /// @param view the memory view being stored ti
    void ReportMemoryStore(const MemoryView* view);

    /// Add a callback for post-step events.
    /// @param callback the callback
    void AddPostStepCallback(PostStepCallback callback) {
        post_step_callbacks_.push_back(callback);
    }

    /// Report a post-step event.
    /// @param invocation the invocation that was just stepped
    void ReportPostStep(const Invocation* invocation);

    /// Add a callback for pre-step events.
    /// @param callback the callback
    void AddPreStepCallback(PreStepCallback callback) { pre_step_callbacks_.push_back(callback); }

    /// Report a pre-step event.
    /// @param invocation the invocation that is about to be stepped
    void ReportPreStep(const Invocation* invocation);

    /// Add a callback for workgroup begin events.
    /// @param callback the callback
    void AddWorkgroupBeginCallback(WorkgroupBeginCallback callback) {
        workgroup_begin_callbacks_.push_back(callback);
    }

    /// Report a workgroup begin event.
    /// @param workgroup the workgroup that it about to begin executing
    void ReportWorkgroupBegin(const Workgroup* workgroup);

    /// Add a callback for workgroup complete events.
    /// @param callback the callback
    void AddWorkgroupCompleteCallback(WorkgroupCompleteCallback callback) {
        workgroup_complete_callbacks_.push_back(callback);
    }

    /// Report a workgroup complete event.
    /// @param workgroup the workgroup that has completed
    void ReportWorkgroupComplete(const Workgroup* workgroup);

  private:
    /// Constructor
    explicit ShaderExecutor(const tint::Program& program);

    /// Initialize the shader executor.
    /// @param entry_point the entry point to run
    /// @param overrides the named pipeline overrides to use
    /// @returns `Success` or a failure reason
    Result Init(std::string entry_point, NamedOverrideList&& overrides);

    /// Evaluate the value of a named pipeline-overridable constant declaration.
    /// @param override_evaluator the helper invocation for evaluating overrides
    /// @param named_override the override declaration
    /// @param overrides the map of values being overridden
    /// @returns `Success` or a failure reason
    Result EvaluateNamedOverride(Invocation& override_evaluator,
                                 const ast::Override* named_override,
                                 NamedOverrideList& overrides);

    const tint::Program& program_;
    ProgramBuilder builder_;
    const ast::Function* entry_point_;
    UVec3 workgroup_count_;
    UVec3 workgroup_size_;
    std::unordered_map<const sem::GlobalVariable*, MemoryView*> bindings_;
    std::unordered_map<const sem::Variable*, const core::constant::Value*> named_overrides_;

    std::unique_ptr<StyledTextPrinter> diag_printer_;

    tint::BlockAllocator<MemoryView> memory_views_;

    std::unique_ptr<core::constant::Eval> const_eval_;
    std::unique_ptr<core::intrinsic::Table<wgsl::intrinsic::Dialect>> intrinsic_table_;

    std::string fatal_error_;

    std::map<UVec3, std::unique_ptr<Workgroup>> pending_groups_;
    std::unique_ptr<Workgroup> current_group_;

    std::vector<BarrierCallback> barrier_callbacks_;
    std::vector<DispatchBeginCallback> dispatch_begin_callbacks_;
    std::vector<DispatchCompleteCallback> dispatch_complete_callbacks_;
    std::vector<ErrorCallback> error_callbacks_;
    std::vector<MemoryLoadCallback> memory_load_callbacks_;
    std::vector<MemoryStoreCallback> memory_store_callbacks_;
    std::vector<PostStepCallback> post_step_callbacks_;
    std::vector<PreStepCallback> pre_step_callbacks_;
    std::vector<WorkgroupBeginCallback> workgroup_begin_callbacks_;
    std::vector<WorkgroupCompleteCallback> workgroup_complete_callbacks_;
};

}  // namespace tint::interp

#endif  // SRC_TINT_INTERP_SHADER_EXECUTOR_H_
