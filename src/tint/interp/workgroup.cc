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

#include "tint/interp/workgroup.h"

#include <unordered_map>
#include <utility>

#include "tint/interp/invocation.h"
#include "tint/interp/memory.h"
#include "tint/interp/shader_executor.h"
#include "tint/lang/wgsl/sem/function.h"

namespace tint::interp {

Workgroup::Workgroup(ShaderExecutor& executor, UVec3 group_id, UVec3 size)
    : executor_(executor), group_id_(group_id), size_(size) {
    auto* func = executor_.Sem().Get(executor_.EntryPoint());

    // Perform workgroup memory allocations.
    Invocation override_helper(executor);
    std::unordered_map<const sem::Variable*, MemoryView*> workgroup_views;
    for (auto* global : func->TransitivelyReferencedGlobals()) {
        if (global->AddressSpace() == core::AddressSpace::kWorkgroup) {
            auto* type = global->Type()->UnwrapRef();
            uint64_t alloc_size = type->Size();
            if (auto* arr = type->As<core::type::Array>()) {
                if (auto* named = arr->Count()->As<sem::NamedOverrideArrayCount>()) {
                    auto* value = executor.GetNamedOverride(named->variable);
                    alloc_size = value->ValueAs<uint32_t>() * arr->ElemType()->Size();
                } else if (auto* unnamed = arr->Count()->As<sem::UnnamedOverrideArrayCount>()) {
                    auto* value =
                        override_helper.EvaluateOverrideExpression(unnamed->expr->Declaration());
                    alloc_size = value->ValueAs<uint32_t>() * arr->ElemType()->Size();
                } else if (!arr->ConstantCount()) {
                    executor.ReportFatalError("unhandled array count in workgroup allocation");
                    return;
                }
            }
            auto alloc = std::make_unique<Memory>(alloc_size);
            auto* view = alloc->CreateView(&executor_, core::AddressSpace::kWorkgroup,
                                           global->Type(), global->Declaration()->source);
            workgroup_views.insert({global, view});
            allocations_.push_back(std::move(alloc));
        }
    }

    // Create the invocations.
    for (uint32_t iz = 0; iz < size.z; iz++) {
        for (uint32_t iy = 0; iy < size.y; iy++) {
            for (uint32_t ix = 0; ix < size.x; ix++) {
                UVec3 local_id = UVec3{ix, iy, iz};
                auto invocation =
                    std::make_unique<Invocation>(executor_, group_id, local_id, workgroup_views);
                ready_.insert({local_id, invocation.get()});
                invocations_.push_back(std::move(invocation));
            }
        }
    }
    bool result = SelectInvocation({0, 0, 0});
    TINT_ASSERT(result);
    executor_.ReportWorkgroupBegin(this);
}

bool Workgroup::SelectInvocation(UVec3 local_id) {
    if (current_invocation_ && current_invocation_->LocalInvocationId() == local_id) {
        // We're already running the requested invocation.
        return true;
    }

    auto itr = ready_.find(local_id);
    if (itr == ready_.end()) {
        // The target invocation has finished, is at a barrier, or the ID wasn't valid.
        return false;
    }

    if (current_invocation_) {
        // Pause the current invocation by inserting it back into the ready map.
        ready_.insert({current_invocation_->LocalInvocationId(), current_invocation_});
    }

    // Select the target invocation and remove it from the ready map.
    current_invocation_ = itr->second;
    ready_.erase(itr);

    return true;
}

void Workgroup::Step() {
    TINT_ASSERT(current_invocation_ != nullptr);

    executor_.ReportPreStep(current_invocation_);
    current_invocation_->Step();
    executor_.ReportPostStep(current_invocation_);

    auto state = current_invocation_->GetState();
    if (state == Invocation::State::kFinished) {
        current_invocation_ = nullptr;
    } else if (state == Invocation::State::kBarrier) {
        barrier_.push_back(current_invocation_);
        current_invocation_ = nullptr;
    } else if (state != Invocation::State::kReady) {
        executor_.ReportFatalError("unhandled invocation state");
        return;
    }

    if (current_invocation_ != nullptr) {
        return;
    }
    if (ready_.empty() && !barrier_.empty()) {
        const ast::CallExpression* first_barrier = barrier_.front()->Barrier();
        const ast::CallExpression* second_barrier = nullptr;
        uint32_t first_barrier_count = 0;
        uint32_t second_barrier_count = 0;
        Invocation* second_barrier_example = nullptr;

        // Clear barriers for every invocation that is waiting, and move them to the ready queue.
        // Track the number of invocations that hit the same barrier while we do this.
        for (auto* waiting : barrier_) {
            if (waiting->Barrier() == first_barrier) {
                first_barrier_count++;
            } else {
                // Make a note of one of the other barriers that have been hit.
                if (second_barrier == nullptr) {
                    second_barrier = waiting->Barrier();
                    second_barrier_count = 1;
                    second_barrier_example = waiting;
                } else {
                    if (waiting->Barrier() == second_barrier) {
                        second_barrier_count++;
                    }
                }
            }

            waiting->ClearBarrier();
            ready_.insert({waiting->LocalInvocationId(), waiting});
        }

        // Check for non-uniform barriers and report an error if we have one.
        if (first_barrier_count != invocations_.size()) {
            diag::List error;
            error.AddError(diag::System::Interpreter, {})
                << "barrier not reached by all invocations in the workgroup\n";
            error.AddNote(diag::System::Interpreter, first_barrier->source)
                << "invocation" << barrier_.front()->LocalInvocationId().Str() << " and "
                << (first_barrier_count - 1) << " other invocations waiting here";
            if (second_barrier != nullptr) {
                // Show an example of an invocation that is waiting at a different barrier.
                error.AddNote(diag::System::Interpreter, second_barrier->source)
                    << "invocation" << second_barrier_example->LocalInvocationId().Str() << " and "
                    << (second_barrier_count - 1) << " other invocations waiting here";
            }
            auto other_barrier_count = barrier_.size() - first_barrier_count - second_barrier_count;
            if (other_barrier_count > 0) {
                // Note how many invocations are waiting are other barriers.
                error.AddNote(diag::System::Interpreter, {})
                    << other_barrier_count << " invocations are waiting at other barriers\n";
            }
            if (barrier_.size() != invocations_.size()) {
                // Note how many invocations have finished the shader completely.
                error.AddNote(diag::System::Interpreter, {})
                    << (invocations_.size() - barrier_.size())
                    << " invocations have finished running the shader\n";
            }
            executor_.ReportError(std::move(error));
        }

        barrier_.clear();
        executor_.ReportBarrier(this, first_barrier);
    }
    if (!ready_.empty()) {
        // Switch to the first invocation in the ready queue.
        bool result = SelectInvocation(ready_.begin()->first);
        TINT_ASSERT(result);
    } else {
        executor_.ReportWorkgroupComplete(this);
    }
}

}  // namespace tint::interp
