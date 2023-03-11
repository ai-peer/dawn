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

#ifndef SRC_TINT_INTERP_WORKGROUP_H_
#define SRC_TINT_INTERP_WORKGROUP_H_

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "tint/interp/uvec3.h"

namespace tint::interp {
class Invocation;
class Memory;
class ShaderExecutor;
}  // namespace tint::interp

namespace tint::interp {

/// A Workgroup object represents a single workgroup in a shader dispatch, and contains a set of
/// invocations.
class Workgroup {
  public:
    /// Constructor.
    /// @param executor the ShaderExecutor
    /// @param group_id the workgroup ID
    /// @param size the workgroup size
    Workgroup(ShaderExecutor& executor, UVec3 group_id, UVec3 size);

    /// @returns the workgroup ID of this workgroup
    const UVec3& GroupId() const { return group_id_; }

    /// @returns the size of this workgroup
    const UVec3& Size() const { return size_; }

    /// @returns the invocation that is currently running
    Invocation* CurrentInvocation() const { return current_invocation_; }

    /// Switch the invocation that is currently executing.
    /// @param local_id the local invocation ID of the invocation to switch to
    /// @returns true on success, false if the ID is invalid or the invocation has already finished
    bool SelectInvocation(UVec3 local_id);

    /// Step the workgroup.
    void Step();

    /// @returns true if the workgroup has completed execution
    bool IsFinished() const {
        return current_invocation_ == nullptr && ready_.empty() && barrier_.empty();
    }

  private:
    ShaderExecutor& executor_;
    UVec3 group_id_;
    UVec3 size_;
    std::vector<std::unique_ptr<Memory>> allocations_;

    std::vector<std::unique_ptr<Invocation>> invocations_;
    Invocation* current_invocation_ = nullptr;
    std::map<UVec3, Invocation*> ready_;
    std::vector<Invocation*> barrier_;
};

}  // namespace tint::interp

#endif  // SRC_TINT_INTERP_WORKGROUP_H_
