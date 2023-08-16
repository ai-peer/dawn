// Copyright 2020 The Tint Authors.
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

#include <iostream>
#include <string>
#include <vector>

#include "glslang/Public/ResourceLimits.h"
#include "glslang/Public/ShaderLang.h"
#include "src/tint/cmd/common/helper.h"

namespace {

void GenerateGlsl(const std::string& data) {
    glslang::InitializeProcess();

    EShLanguage lang = EShLangVertex;
    glslang::TShader shader(lang);

    const char* strings[1] = {data.c_str()};
    int lengths[1] = {static_cast<int>(data.length())};

    shader.setStringsWithLengths(strings, lengths, 1);
    shader.setEntryPoint("main");

    bool glslang_result =
        shader.parse(GetDefaultResources(), 310, EEsProfile, false, false, EShMsgDefault);
    if (!glslang_result) {
        std::cerr << "Error parsing GLSL shader:\n"
                  << shader.getInfoLog() << "\n"
                  << shader.getInfoDebugLog() << "\n";
    }

    glslang::FinalizeProcess();
}

}  // namespace

int main(int argc, const char** argv) {
    if (argc < 2) {
        std::cerr << "missing file\n";
        return 1;
    }

    std::vector<char> in;
    if (!tint::cmd::ReadFile(argv[1], &in)) {
        std::cerr << "failed to read input file: " << argv[1] << "\n";
        return 1;
    }

    std::string s{in.data(), in.size()};

    //   glslang::InitializeProcess();
    for (size_t i = 0; i < 1000; ++i) {
        GenerateGlsl(s);
    }
    //    glslang::FinalizeProcess();

    return 0;
}
