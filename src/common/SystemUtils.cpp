// Copyright 2019 The Dawn Authors
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

#include "common/SystemUtils.h"

#if defined(DAWN_PLATFORM_WINDOWS)
#    include <Windows.h>
#elif defined(DAWN_PLATFORM_LINUX)
#    include <limits.h>
#    include <unistd.h>
#    include <cstdlib>
#elif defined(DAWN_PLATFORM_MACOS)
#    include <mach-o/dyld.h>
#    include <vector>
#endif

#include <array>
#include <string>

#if defined(DAWN_PLATFORM_WINDOWS)
bool SetEnvironmentVar(const char* variableName, const char* value) {
    return (SetEnvironmentVariableA(variableName, value) == TRUE);
}
#elif defined(DAWN_PLATFORM_POSIX)
bool SetEnvironmentVar(const char* variableName, const char* value) {
    return (setenv(variableName, value, 1) == 0);
}
#else
#    error "Implement SetEnvironmentVar for your platform."
#endif

#if defined(DAWN_PLATFORM_WINDOWS)
std::string GetExecutablePath() {
    std::array<char, MAX_PATH> executableFileBuf;
    DWORD executablePathLen = GetModuleFileNameA(nullptr, executableFileBuf.data(),
                                                 static_cast<DWORD>(executableFileBuf.size()));
    return executablePathLen > 0 ? std::string(executableFileBuf.data()) : "";
}
#elif defined(DAWN_PLATFORM_LINUX)
std::string GetExecutablePath() {
    std::array<char, PATH_MAX> path;
    ssize_t result = readlink("/proc/self/exe", path.data(), PATH_MAX - 1);
    if (result < 0 || static_cast<size_t>(result) >= PATH_MAX - 1) {
        return "";
    }
    path[result] = '\0';
    return path.data();
}
#elif defined(DAWN_PLATFORM_MACOS)
std::string GetExecutablePath() {
    std::string result;

    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);

    std::vector<char> buffer;
    buffer.resize(size + 1);

    _NSGetExecutablePath(buffer.data(), &size);
    buffer[size] = '\0';

    if (!strrchr(buffer.data(), kDawnSeparator)) {
        return "";
    }
    return buffer.data();
}
#elif defined(DAWN_PLATFORM_FUCHSIA)
std::string GetExecutablePath() {
    // TODO(hao.x.li): Implement on Fuchsia
    return "";
}
#else
#    error "Implement GetExecutablePath for your platform."
#endif

const char* GetExecutableDirectory() {
    std::string exePath = GetExecutablePath();
    size_t lastPathSepLoc = exePath.find_last_of(kDawnSeparator);
    const static std::string& exeDir =
        (lastPathSepLoc != std::string::npos) ? exePath.substr(0, lastPathSepLoc + 1) : "";
    return exeDir.c_str();
}
