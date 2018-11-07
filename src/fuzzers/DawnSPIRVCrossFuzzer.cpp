// Copyright 2018 The Dawn Authors
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

#include <csetjmp>
#include <csignal>
#include <cstdint>
#include <string>
#include <vector>

#include "DawnSPIRVCrossFuzzer.h"

namespace {

    std::jmp_buf jump_buffer;
    void (*old_signal_handler)(int);

    // Handler to trap SIGABRT, so that it doesn't crash the fuzzer. Currently the
    // code being fuzzed uses abort() to report errors like bad input instead of
    // returning an error code. This will be changing in the future.
    //
    // TODO(rharrison): Remove all of this SIGABRT trapping once SPRIV-Cross has
    // been changed to not use abort() for reporting errors.
    [[noreturn]] static void sigabrt_trap(int sig) {
        if (sig != SIGABRT && old_signal_handler != nullptr) {
            old_signal_handler(sig);
        } else {
            raise(sig);
        }

        std::longjmp(jump_buffer, 1);
    }

}  // namespace

namespace DawnSPIRVCrossFuzzer {

    int Run(const uint8_t* data, size_t size, int (*task)(std::vector<uint32_t>)) {
        if (!data || size < 1)
            return 0;

        std::vector<uint32_t> input;
        input.resize(size >> 2);
        if (input.size() < 5)
            return 0;

        size_t count = 0;
        for (size_t i = 0; (i + 3) < size; i += 4) {
            input[count++] =
                data[i] | (data[i + 1] << 8) | (data[i + 2] << 16) | (data[i + 3]) << 24;
        }

        // Setup the SIGABRT trap
        old_signal_handler = signal(SIGABRT, sigabrt_trap);
        if (old_signal_handler == SIG_ERR)
            abort();

        if (setjmp(jump_buffer) == 0) {
            task(input);
        }

        // Restore the previous signal handler
        signal(SIGABRT, old_signal_handler);
        return 0;
    }

}  // namespace DawnSPIRVCrossFuzzer
