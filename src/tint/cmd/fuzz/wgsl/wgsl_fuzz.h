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

#ifndef SRC_TINT_CMD_FUZZ_WGSL_WGSL_FUZZ_H_
#define SRC_TINT_CMD_FUZZ_WGSL_WGSL_FUZZ_H_

#include <string>

#include "src/tint/utils/containers/slice.h"
#include "src/tint/utils/macros/static_init.h"
#include "src/tint/utils/math/crc32.h"

namespace tint::fuzz::wgsl {

using FuzzerFn = void(Slice<const uint8_t> input);

void Register(std::string_view name, uint16_t unique_id, FuzzerFn* function);

bool Run(uint16_t unique_id, Slice<const uint8_t> input);

/// @returns the name of the currently running fuzzer
std::string_view CurrentlyRunning();

#define TINT_WGSL_FUZZER(FUNCTION)                                                             \
    template <>                                                                                \
    struct ::tint::fuzz::wgsl::CheckIDIsUnique<tint::CRC32(#FUNCTION) & 0xffff> {};            \
    TINT_STATIC_INIT(::tint::fuzz::wgsl::Register, #FUNCTION, tint::CRC32(#FUNCTION) & 0xffff, \
                     FUNCTION)

template <uint16_t UNIQUE_ID>
struct CheckIDIsUnique {};

}  // namespace tint::fuzz::wgsl

#endif  // SRC_TINT_CMD_FUZZ_WGSL_WGSL_FUZZ_H_
