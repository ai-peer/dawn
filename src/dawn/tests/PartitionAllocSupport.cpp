// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#include "dawn/tests/PartitionAllocSupport.h"

#include "dawn/common/Assert.h"
#include "dawn/common/Log.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
#pragma GCC diagnostic ignored "-Wzero-length-array"
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#include "partition_alloc/dangling_raw_ptr_checks.h"
#include "partition_alloc/shim/allocator_shim.h"
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#pragma GCC diagnostic pop

namespace dawn {

void InitializePartitionAllocForTesting() {
    // 64bit refcount is required to check for dangling pointers.
    constexpr int backupref_ptr_refcount_size = 8;
    constexpr int partition_alloc_scheduler_loop_quarantine_capacity_in_byte = 0;
    allocator_shim::ConfigurePartitions(
        allocator_shim::EnableBrp(true), allocator_shim::EnableMemoryTagging(false),
        partition_alloc::TagViolationReportingMode::kDisabled,
        allocator_shim::SplitMainPartition(true),
        allocator_shim::UseDedicatedAlignedPartition(false), backupref_ptr_refcount_size,
        allocator_shim::BucketDistribution::kNeutral,
        partition_alloc_scheduler_loop_quarantine_capacity_in_byte);
    allocator_shim::internal::PartitionAllocMalloc::Allocator()->EnableThreadCacheIfSupported();
}

void InitializeDanglingPointerDetectorForTesting() {
    partition_alloc::SetDanglingRawPtrDetectedFn([](uintptr_t ptr) {
        ErrorLog() << "DanglingPointerDetector: A pointer becomes dangling!";
        DAWN_CHECK(false);
        // TODO(arthursonzogni): It would have been nice recording and join
        // associated StackTrace here and below, and display them to developers
        // as error messages. For now, there aren't support StackTraces
        // implementations in dawn.
    });

    partition_alloc::SetDanglingRawPtrReleasedFn([](uintptr_t ptr) {
        DAWN_UNREACHABLE();  // It should have crashed in the previous handlers.
    });
}

}  // namespace dawn
