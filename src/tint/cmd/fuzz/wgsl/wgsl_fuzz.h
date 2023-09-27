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

/// Registers the fuzzer function with the WGSL fuzzer executable.
/// @param name the name of the fuzzer
/// @param unique_id the unique identifier of the fuzzer
/// @param function the fuzzer function
void Register(std::string_view name, uint16_t unique_id, FuzzerFn* function);

/// Runs the fuzzer with the unique identifier @p unique_id
/// @param unique_id the unique identifier of the fuzzer
/// @param input the input data for the fuzzer
/// @return true if the fuzzer exercised code which is worthy of storing to the fuzzer corpus.
bool Run(uint16_t unique_id, Slice<const uint8_t> input);

/// @returns the name of the currently running fuzzer
std::string_view CurrentlyRunning();

#define TINT_WGSL_FUZZER(FUNCTION)                                                             \
    /* link-time check that the fuzzer identifier is unique */                                 \
    template <>                                                                                \
    int ::tint::fuzz::wgsl::CheckIDIsUnique<tint::CRC32(#FUNCTION)& 0xffff> = 0;               \
    /* register the fuzzer function on static initialization */                                \
    TINT_STATIC_INIT(::tint::fuzz::wgsl::Register, #FUNCTION, tint::CRC32(#FUNCTION) & 0xffff, \
                     FUNCTION)

/// A templated symbol used to check at link-time that two WGSL fuzzers do not use the same hashed
/// `unique_id`. If you get the linker error:
/// `multiple definition of `tint::fuzz::wgsl::CheckIDIsUnique<X>';`
/// Then please change the name of the *new* function being fuzzed.
template <int UNIQUE_ID>
int CheckIDIsUnique;

}  // namespace tint::fuzz::wgsl

#endif  // SRC_TINT_CMD_FUZZ_WGSL_WGSL_FUZZ_H_
