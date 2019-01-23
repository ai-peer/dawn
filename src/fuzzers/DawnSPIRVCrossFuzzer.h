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

#include <cstdint>
#include <functional>
#include <vector>

#include "shaderc/spvc.hpp"
#include "spirv-cross/spirv_glsl.hpp"
#include "spirv-cross/spirv_hlsl.hpp"

namespace DawnSPIRVCrossFuzzer {

    struct CombinedOptions {  // XXX won't need this struct after all fuzzers converted
        spirv_cross::CompilerGLSL::Options glsl;
        spirv_cross::CompilerHLSL::Options hlsl;
        shaderc_spvc::CompileOptions spvc;  // XXX can use this instead, it has all the options
    };

    using Task = std::function<int(const std::vector<uint32_t>&)>;
    template <class Options>
    using TaskWithOptions = std::function<int(const std::vector<uint32_t>&, Options)>;

    // Used to wrap code that may fire a SIGABRT. Do not allocate anything local within |exec|, as
    // it is not guaranteed to return.
    void ExecuteWithSignalTrap(std::function<void()> exec);

    // Used to fuzz by mutating the input data, but with fixed options to the compiler
    int Run(const uint8_t* data, size_t size, Task task);

    // Used to fuzz by mutating both the input data and options to the compiler
    template <class Options>
    int RunWithOptions(const uint8_t* data, size_t size, TaskWithOptions<Options> task);

}  // namespace DawnSPIRVCrossFuzzer
