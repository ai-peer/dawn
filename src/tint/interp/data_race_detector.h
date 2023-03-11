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

#ifndef SRC_TINT_INTERP_DATA_RACE_DETECTOR_H_
#define SRC_TINT_INTERP_DATA_RACE_DETECTOR_H_

#include <unordered_map>
#include <vector>

#include "tint/interp/uvec3.h"
#include "tint/lang/core/address_space.h"
#include "tint/utils/diagnostic/source.h"
#include "tint/utils/math/hash.h"

// Forward Declarations
namespace tint::ast {
class CallExpression;
class Node;
}  // namespace tint::ast
namespace tint::interp {
class Invocation;
class MemoryView;
class ShaderExecutor;
class Workgroup;
}  // namespace tint::interp

namespace tint::interp {

/// The DataRaceDetector class implements a dynamic data race detector that works by tracking memory
/// accesses and checking for conflicts at synchronization points.
class DataRaceDetector {
  public:
    /// Construct a data race detector for a given shader executor.
    /// @param executor the shader executor to attach to
    explicit DataRaceDetector(ShaderExecutor& executor);

  private:
    ShaderExecutor& executor_;

    // Handlers for events that occur during shader execution.
    void Barrier(const Workgroup* workgroup, const ast::CallExpression* call);
    void MemoryLoad(const MemoryView* view);
    void MemoryStore(const MemoryView* view);
    void WorkgroupBegin(const Workgroup* workgroup);
    void WorkgroupComplete(const Workgroup* workgroup);

    /// MemoryAccessKind is used to distinguish between load and store operations.
    enum class MemoryAccessKind { kLoad, kStore };

    /// MemoryAccess represents a memory access to a single byte of memory.
    struct MemoryAccess {
        UVec3 workgroup;         // The workgroup's ID.
        UVec3 invocation;        // The invocation's local ID.
        const ast::Node* cause;  // The AST node that caused the access.
        MemoryAccessKind kind;   // The kind of access (load vs store).
        uint64_t byte_offset;    // The offset in bytes from the root of the memory allocation.
        uint64_t byte_size;      // The size in bytes of the access.

        // Flag to determine if this is a write to a single component of a vector.
        bool is_vector_component_write = false;
    };

    /// MemoryAccessKey is a key used to determine the uniqueness of a memory access location.
    struct MemoryAccessKey {
        const MemoryView* view;  // The root memory view.
        uint64_t offset;         // The offset in bytes from the start of the allocation.
        bool operator==(const MemoryAccessKey& rhs) const {
            return view == rhs.view && offset == rhs.offset;
        }
    };

    /// Helper struct to allow MemoryAccessKey to be used as the key in an unordered_map.
    struct MemoryAccessHasher {
        size_t operator()(const MemoryAccessKey& key) const {
            return tint::Hash(key.view, key.offset);
        }
    };
    using MemoryAccessMap = std::unordered_map<MemoryAccessKey, MemoryAccess, MemoryAccessHasher>;

    /// Register a memory access for an invocation.
    /// @param view the memory view being accessed
    /// @param kind the kind of access being performed
    void RegisterAccess(const MemoryView* view, MemoryAccessKind kind);

    /// Merge a memory access into an access map, optionally check for data races.
    /// @param access_key the memory access key
    /// @param access the memory access to merge
    /// @param access_map the memory access map
    /// @param check_for_races `true` to check for memory races when merging
    void MergeAccess(MemoryAccessKey access_key,
                     MemoryAccess access,
                     MemoryAccessMap& access_map,
                     bool check_for_races);

    /// Synchronize memory accesses within a workgroup.
    /// @param workgroup the workgroup to synchronize accesses for
    /// @param addrspace the address space to synchronize
    void SyncWorkgroup(const Workgroup* workgroup, core::AddressSpace addrspace);

    /// InvocationAccesses contains access maps for workgroup and storage buffer memory.
    struct InvocationAccesses {
        MemoryAccessMap workgroup_accesses_;
        MemoryAccessMap storage_accesses_;
    };

    // Access maps that record memory accesses at various scopes.
    std::unordered_map<UVec3, std::vector<InvocationAccesses>> per_invocation_accesses_;
    std::unordered_map<UVec3, MemoryAccessMap> per_group_storage_buffer_accesses_;
    MemoryAccessMap inter_group_storage_buffer_accesses_;

    /// Race represents a data race, and tracks whether or not it has been emitted yet.
    struct Race {
        const MemoryView* root_view;  // The root memory view.
        MemoryAccess a;               // The first access.
        MemoryAccess b;               // The second access.
        bool emitted = false;         // Tracks whether or not the race has been emitted.
    };

    /// RaceKey is a key used to determine the uniqueness of data race.
    struct RaceKey {
        Source::Location decl;
        const ast::Node* cause_a;
        const ast::Node* cause_b;
        bool operator==(const RaceKey& rhs) const {
            return decl == rhs.decl && cause_a == rhs.cause_a && cause_b == rhs.cause_b;
        }
    };

    /// Helper struct to allow RaceKey to be used as the key in an unordered_map.
    struct RaceHasher {
        size_t operator()(const RaceKey& key) const {
            return tint::Hash(key.decl.line, key.decl.column, key.cause_a, key.cause_b);
        }
    };

    /// Record a race to be emitted later.
    /// @param root_view the memory view that represents the variable being accessed
    /// @param a the first memory access
    /// @param b the second memory access
    void RecordRace(const MemoryView* root_view, MemoryAccess a, MemoryAccess b);

    /// Emit any races that have been recorded.
    void EmitRaces();

    /// The data races that have been recorded by this DataRaceDetector.
    std::unordered_map<RaceKey, Race, RaceHasher> races_;
};

}  // namespace tint::interp

#endif  // SRC_TINT_INTERP_DATA_RACE_DETECTOR_H_
