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

#include "tint/interp/shader_executor.h"

#include <cstdio>
#include <utility>

#include "tint/interp/invocation.h"
#include "tint/interp/workgroup.h"
#include "tint/lang/core/constant/eval.h"
#include "tint/lang/core/constant/scalar.h"
#include "tint/lang/wgsl/sem/function.h"
#include "tint/lang/wgsl/sem/module.h"
#include "tint/utils/diagnostic/diagnostic.h"
#include "tint/utils/diagnostic/formatter.h"
#include "tint/utils/rtti/switch.h"
#include "tint/utils/text/styled_text_printer.h"

namespace tint::interp {

tint::Result<std::unique_ptr<ShaderExecutor>, std::string> ShaderExecutor::Create(
    const tint::Program& program,
    std::string entry_point,
    NamedOverrideList&& overrides) {
    auto executor = std::unique_ptr<ShaderExecutor>(new ShaderExecutor(program));
    auto result = executor->Init(entry_point, std::move(overrides));
    if (result != Success) {
        return result.Failure();
    }
    return executor;
}

ShaderExecutor::ShaderExecutor(const tint::Program& program) : program_(program) {}

ShaderExecutor::~ShaderExecutor() = default;

void ShaderExecutor::FlushErrors() {
    if (builder_.Diagnostics().Count() > 0) {
        ReportError(std::move(builder_.Diagnostics()));
        builder_.Diagnostics() = {};
    }
}

namespace {
/// Make a formatted error from a message and an optional source location.
/// @param msg the error message
/// @param source the optional source location
/// @returns the formatted error string
std::string MakeError(std::string msg, Source source = {}) {
    diag::List list;
    list.AddError(diag::System::Interpreter, source) << msg;
    diag::Formatter::Style style{};
    style.print_newline_at_end = false;
    diag::Formatter formatter{style};
    return formatter.Format(list).Plain();
}
}  // namespace

void ShaderExecutor::ReportFatalError(std::string msg, Source source /* = {} */) {
    if (!fatal_error_.empty()) {
        // Avoid cascading from previous fatal errors.
        return;
    }
    fatal_error_ = MakeError(msg, source);
}

ShaderExecutor::Result ShaderExecutor::Init(std::string entry_point,
                                            NamedOverrideList&& overrides) {
    diag_printer_ = StyledTextPrinter::Create(stderr);

    // Set up the program builder and related objects used to evaluate expressions.
    builder_ = ProgramBuilder::Wrap(program_);
    const_eval_ = std::make_unique<core::constant::Eval>(builder_.constants, builder_.Diagnostics(),
                                                         /* use_runtime_semantics */ true);
    intrinsic_table_ = std::make_unique<core::intrinsic::Table<wgsl::intrinsic::Dialect>>(
        builder_.Types(), builder_.Symbols());

    // Clear any warnings that might be in the diagnostics before we start.
    builder_.Diagnostics() = {};

    // Find the target entry point.
    entry_point_ = nullptr;
    for (auto* func : builder_.AST().Functions()) {
        if (func->name->symbol.Name() == entry_point) {
            entry_point_ = func;
            break;
        }
    }
    if (!entry_point_) {
        return MakeError("entry point '" + entry_point + "' not found in module");
    }
    if (entry_point_->PipelineStage() != ast::PipelineStage::kCompute) {
        return MakeError("function '" + entry_point + "' is not a compute shader");
    }

    auto& referenced_globals = builder_.Sem().Get(entry_point_)->TransitivelyReferencedGlobals();

    // Evaluate named pipeline-overridable constants.
    Invocation override_evaluator(*this);
    for (auto* decl : builder_.Sem().Module()->DependencyOrderedDeclarations()) {
        if (auto* named_override = decl->As<ast::Override>()) {
            if (!referenced_globals.Contains(builder_.Sem().Get(named_override))) {
                continue;
            }
            auto eval = EvaluateNamedOverride(override_evaluator, named_override, overrides);
            if (eval != Success) {
                return eval.Failure();
            }
        }
    }

    // Get the values of the workgroup size attribute, which may be constants or pipeline overrides.
    std::string err;
    auto get_wgsize_dim = [&](const ast::Expression* ast) {
        if (ast == nullptr) {
            // Default to 1 if not specified.
            return 1u;
        }
        auto* expr = builder_.Sem().GetVal(ast);
        if (expr->Stage() == core::EvaluationStage::kConstant) {
            return expr->ConstantValue()->ValueAs<uint32_t>();
        }
        if (expr->Stage() == core::EvaluationStage::kOverride) {
            return override_evaluator.EvaluateOverrideExpression(ast)->ValueAs<uint32_t>();
        }
        err = MakeError("invalid evaluation stage for workgroup size expression", ast->source);
        return 0u;
    };
    auto* wgsize_attr = ast::GetAttribute<ast::WorkgroupAttribute>(entry_point_->attributes);
    workgroup_size_.x = get_wgsize_dim(wgsize_attr->x);
    workgroup_size_.y = get_wgsize_dim(wgsize_attr->y);
    workgroup_size_.z = get_wgsize_dim(wgsize_attr->z);
    if (!err.empty()) {
        return err;
    }

    return Success;
}

ShaderExecutor::Result ShaderExecutor::EvaluateNamedOverride(Invocation& override_evaluator,
                                                             const ast::Override* named_override,
                                                             NamedOverrideList& overrides) {
    auto* var = builder_.Sem().Get(named_override);
    auto name = named_override->name->symbol.Name();

    // Use the name of the variable as the key, unless the @id attribute is specified.
    auto key = name;
    if (auto* id = ast::GetAttribute<ast::IdAttribute>(named_override->attributes)) {
        auto* value = builder_.Sem().GetVal(id->expr)->ConstantValue();
        key = std::to_string(value->ValueAs<uint32_t>());
    }

    if (overrides.count(key)) {
        // The constant has been overridden, so use the specified value.
        auto value = overrides.at(key);
        auto* result = Switch(
            var->Type(),
            [&](const core::type::Bool*) {
                return builder_.constants.Get(!std::equal_to<double>()(value, 0.0));
            },
            [&](const core::type::I32*) { return builder_.constants.Get(core::i32(value)); },
            [&](const core::type::U32*) { return builder_.constants.Get(core::u32(value)); },
            [&](const core::type::F32*) { return builder_.constants.Get(core::f32(value)); });
        if (result == nullptr) {
            return MakeError("unhandled pipeline-override type", named_override->source);
        }

        named_overrides_[var] = result;
    } else if (named_override->initializer) {
        // Evaluate the initializer using the helper invocation.
        auto* result = override_evaluator.EvaluateOverrideExpression(named_override->initializer);
        if (result == nullptr) {
            // TODO(jrprice): This should be non-fatal and done at pipeline creation time so that we
            // can surface validation errors to the user.
            return MakeError("failed to evaluate initializer for '" + name + "'",
                             named_override->source);
        }
        named_overrides_[var] = result;
    } else {
        // TODO(jrprice): This should be non-fatal and done at pipeline creation time so that we
        // can surface validation errors to the user.
        return MakeError("missing pipeline-override value for '" + name + "'",
                         named_override->source);
    }

    return Success;
}

ShaderExecutor::Result ShaderExecutor::Run(UVec3 workgroup_count, BindingList&& bindings) {
    workgroup_count_ = workgroup_count;

    // Generate memory views for each resource binding.
    auto& referenced_globals = builder_.Sem().Get(entry_point_)->TransitivelyReferencedGlobals();
    for (auto* global : referenced_globals) {
        auto bp = global->Attributes().binding_point;
        switch (global->AddressSpace()) {
            case core::AddressSpace::kUndefined:
            case core::AddressSpace::kPrivate:
            case core::AddressSpace::kWorkgroup:
                break;
            case core::AddressSpace::kStorage:
            case core::AddressSpace::kUniform: {
                if (!bindings.count(bp.value())) {
                    return MakeError("missing buffer binding for @group(" +
                                     std::to_string(bp->group) + ") @binding(" +
                                     std::to_string(bp->binding) + ")");
                }
                auto& binding = bindings.at(bp.value());
                if (!binding.buffer) {
                    return MakeError("invalid binding resource for @group(" +
                                     std::to_string(bp->group) + ") @binding(" +
                                     std::to_string(bp->binding) + ")");
                }
                // Create the memory view and add it to the shader executor.
                auto* view = binding.buffer->CreateView(
                    this, global->AddressSpace(), global->Type()->UnwrapRef(),
                    binding.buffer_offset, binding.buffer_size, global->Declaration()->source);
                bindings_[global] = view;
                break;
            }
            default:
                return MakeError("unhandled binding resource address space",
                                 global->Declaration()->source);
        }
    }

    // Build a set of pending workgroup IDs for the dispatch.
    // We will create the Workgroup objects as needed during execution.
    for (uint32_t wgz = 0; wgz < workgroup_count.z; wgz++) {
        for (uint32_t wgy = 0; wgy < workgroup_count.y; wgy++) {
            for (uint32_t wgx = 0; wgx < workgroup_count.x; wgx++) {
                pending_groups_.insert({UVec3{wgx, wgy, wgz}, nullptr});
            }
        }
    }

    // Run until all groups have finished.
    ReportDispatchBegin();
    while (true) {
        if (!current_group_) {
            if (pending_groups_.empty()) {
                // No more groups - we're done.
                break;
            } else {
                // Select the next group from the pending group map.
                bool result = SelectWorkgroup(pending_groups_.begin()->first);
                TINT_ASSERT(result);
            }
        }

        // Check for fatal errors.
        if (!fatal_error_.empty()) {
            return fatal_error_;
        }

        current_group_->Step();
        FlushErrors();

        if (current_group_->IsFinished()) {
            current_group_.reset();
        }
    }
    ReportDispatchComplete();

    return Success;
}

const Invocation* ShaderExecutor::CurrentInvocation() const {
    if (current_group_) {
        return current_group_->CurrentInvocation();
    }
    return nullptr;
}

bool ShaderExecutor::SelectWorkgroup(UVec3 group_id) {
    if (current_group_ && current_group_->GroupId() == group_id) {
        // We're already running the requested group.
        return true;
    }

    auto itr = pending_groups_.find(group_id);
    if (itr == pending_groups_.end()) {
        // The target group has finished or the ID wasn't valid.
        return false;
    }

    if (current_group_) {
        // Pause the current group by inserting it back into the pending group map.
        pending_groups_.insert({current_group_->GroupId(), std::move(current_group_)});
    }

    if (itr->second) {
        // The group has already started running, so resume it.
        current_group_ = std::move(itr->second);
    } else {
        // The group has not been created yet, so create it.
        current_group_ = std::make_unique<Workgroup>(*this, itr->first, workgroup_size_);
    }
    pending_groups_.erase(itr);

    return true;
}

const Source::File& ShaderExecutor::SourceFile() const {
    // This is a bit of a hack to get a handle to the source file, but we know that there is at
    // least one global declaration since there is a valid ShaderExecutor.
    TINT_ASSERT(!program_.AST().GlobalDeclarations().IsEmpty());
    return *(program_.AST().GlobalDeclarations()[0]->source.file);
}

void ShaderExecutor::ReportBarrier(const Workgroup* workgroup, const ast::CallExpression* call) {
    for (auto& callback : barrier_callbacks_) {
        callback(workgroup, call);
    }
}

void ShaderExecutor::ReportDispatchBegin() {
    for (auto& callback : dispatch_begin_callbacks_) {
        callback();
    }
}

void ShaderExecutor::ReportDispatchComplete() {
    for (auto& callback : dispatch_complete_callbacks_) {
        callback();
    }
}

void ShaderExecutor::ReportError(diag::List&& error) {
    if (!error_callbacks_.empty()) {
        for (auto& callback : error_callbacks_) {
            callback(std::move(error));
        }
    } else {
        // No callbacks are registered, so display the diagnostic to stderr.
        if (auto* invocation = CurrentInvocation()) {
            auto itr = error.begin();
            auto first = *itr;

            // Add the currently running invocation to the first diagnostic in the list.
            auto& local = invocation->LocalInvocationId();
            auto& group = invocation->WorkgroupId();
            first.message << "\nwhile running local_invocation_id" + local.Str() + " workgroup_id" +
                                 group.Str();

            diag::List new_error;
            new_error.Add(std::move(first));
            for (++itr; itr != error.end(); itr++) {
                new_error.Add(diag::Diagnostic(*itr));
            }

            error = new_error;
        }

        // No callback set, so send the error to default printer.
        diag::Formatter::Style style{};
        diag::Formatter formatter{style};
        diag_printer_->Print(formatter.Format(error));
    }
}

void ShaderExecutor::ReportMemoryLoad(const MemoryView* view) {
    for (auto& callback : memory_load_callbacks_) {
        callback(view);
    }
}

void ShaderExecutor::ReportMemoryStore(const MemoryView* view) {
    for (auto& callback : memory_store_callbacks_) {
        callback(view);
    }
}

void ShaderExecutor::ReportPostStep(const Invocation* invocation) {
    for (auto& callback : post_step_callbacks_) {
        callback(invocation);
    }
}

void ShaderExecutor::ReportPreStep(const Invocation* invocation) {
    for (auto& callback : pre_step_callbacks_) {
        callback(invocation);
    }
}

void ShaderExecutor::ReportWorkgroupBegin(const Workgroup* workgroup) {
    for (auto& callback : workgroup_begin_callbacks_) {
        callback(workgroup);
    }
}

void ShaderExecutor::ReportWorkgroupComplete(const Workgroup* workgroup) {
    for (auto& callback : workgroup_complete_callbacks_) {
        callback(workgroup);
    }
}

}  // namespace tint::interp
