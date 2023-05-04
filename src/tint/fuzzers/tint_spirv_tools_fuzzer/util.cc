// Copyright 2021 The Tint Authors.
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

#include <fstream>
#include <iostream>
#include <optional>

#include "src/tint/fuzzers/tint_spirv_tools_fuzzer/util.h"

namespace tint::fuzzers::spvtools_fuzzer::util {
namespace {

bool WriteBinary(std::string_view path, const uint8_t* data, size_t size) {
    std::ofstream spv(std::string(path), std::ios::binary);
    return spv &&
           spv.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
}

void LogError(uint32_t index,
              std::string_view type,
              std::string_view message,
              std::string_view path,
              const uint8_t* data,
              size_t size,
              std::string_view wgsl) {
    std::cout << index << " | " << type << ": " << message << std::endl;

    if (!path.empty()) {
        auto prefix = std::string(path) + std::to_string(index);
        std::ofstream(prefix + ".log") << message << std::endl;

        WriteBinary(prefix + ".spv", data, size);

        if (!wgsl.empty()) {
            std::ofstream(prefix + ".wgsl") << wgsl << std::endl;
        }
    }
}

}  // namespace

spvtools::MessageConsumer GetBufferMessageConsumer(std::stringstream* buffer) {
    return [buffer](spv_message_level_t level, const char*, const spv_position_t& position,
                    const char* message) {
        std::string status;
        switch (level) {
            case SPV_MSG_FATAL:
            case SPV_MSG_INTERNAL_ERROR:
            case SPV_MSG_ERROR:
                status = "ERROR";
                break;
            case SPV_MSG_WARNING:
            case SPV_MSG_INFO:
            case SPV_MSG_DEBUG:
                status = "INFO";
                break;
        }
        *buffer << status << " " << position.line << ":" << position.column << ":" << position.index
                << ": " << message << std::endl;
    };
}

void LogMutatorError(const Mutator& mutator, std::string_view error_dir) {
    static uint32_t mutator_count = 0;
    if (error_dir.empty()) {
        mutator.LogErrors(error_dir, mutator_count++);
    } else {
        std::string error_path = std::string(error_dir) + "/mutator/";
        mutator.LogErrors(error_path, mutator_count++);
    }
}

void LogWgslError(std::string_view message,
                  const uint8_t* data,
                  size_t size,
                  std::string_view wgsl,
                  OutputFormat output_format,
                  std::string_view error_dir) {
    static uint32_t wgsl_count = 0;
    std::string error_type;
    switch (output_format) {
        case OutputFormat::kSpv:
            error_type = "WGSL -> SPV";
            break;
        case OutputFormat::kMSL:
            error_type = "WGSL -> MSL";
            break;
        case OutputFormat::kHLSL:
            error_type = "WGSL -> HLSL";
            break;
        case OutputFormat::kWGSL:
            error_type = "WGSL -> WGSL";
            break;
    }
    if (error_dir.empty()) {
        LogError(wgsl_count++, error_type, message, error_dir, data, size, wgsl);
    } else {
        auto error_path = std::string(error_dir) + "/wgsl/";
        LogError(wgsl_count++, error_type, message, error_path, data, size, wgsl);
    }
}

void LogSpvError(std::string_view message,
                 const uint8_t* data,
                 size_t size,
                 std::string_view error_dir) {
    static uint32_t spv_count = 0;
    if (error_dir.empty()) {
        LogError(spv_count++, "SPV -> WGSL", message, error_dir, data, size, "");
    } else {
        auto error_path = std::string(error_dir) + "/spv/";
        LogError(spv_count++, "SPV -> WGSL", message, error_path, data, size, "");
    }
}

bool ReadBinary(std::string_view path, std::vector<uint32_t>* out) {
    if (!out) {
        return false;
    }

    std::ifstream file(std::string(path), std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }

    size_t size = static_cast<size_t>(file.tellg());
    if (!file) {
        return false;
    }

    file.seekg(0);
    if (!file) {
        return false;
    }

    std::vector<char> binary(size);
    if (!file.read(binary.data(), size)) {
        return false;
    }

    out->resize(binary.size() / sizeof(uint32_t));
    std::memcpy(out->data(), binary.data(), binary.size());
    return true;
}

bool WriteBinary(std::string_view path, const std::vector<uint32_t>& binary) {
    return WriteBinary(path, reinterpret_cast<const uint8_t*>(binary.data()),
                       binary.size() * sizeof(uint32_t));
}

}  // namespace tint::fuzzers::spvtools_fuzzer::util
