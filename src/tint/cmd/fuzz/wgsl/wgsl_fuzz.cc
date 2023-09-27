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

#include "src/tint/cmd/fuzz/wgsl/wgsl_fuzz.h"

#include <iostream>

#include "src/tint/utils/containers/hashmap.h"

namespace tint::fuzz::wgsl {
namespace {

struct Fuzzer {
    std::string_view name;
    FuzzerFn* fn = nullptr;
};

Hashmap<uint16_t, Fuzzer, 32> fuzzers;
std::string_view currently_running;

}  // namespace

void Register(std::string_view name, uint16_t unique_id, FuzzerFn* function) {
    if (!fuzzers.Add(unique_id, Fuzzer{name, function})) {
        std::cerr << "WGSL fuzzer hash collision" << std::endl;
        __builtin_trap();
    }
}

std::string_view CurrentlyRunning() {
    return currently_running;
}

bool Run(uint16_t unique_id, Slice<const uint8_t> input) {
    if (auto fuzzer = fuzzers.Get(unique_id)) {
        currently_running = fuzzer->name;
        fuzzer->fn(input);
        currently_running = "";
        return true;
    }
    return false;
}

}  // namespace tint::fuzz::wgsl
