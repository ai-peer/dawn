// Copyright 2022 The Tint Authors.
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

#include "src/tint/utils/debugger.h"

#ifdef TINT_ENABLE_BREAK_IN_DEBUGGER

#ifdef _MSC_VER
#include <Windows.h>
#elif defined(__linux__)
#include <signal.h>
#include <fstream>
#include <string>
#elif defined(__APPLE__)
#include <stdbool.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

bool tint::debugger::Attached() {
#ifdef _MSC_VER
    return ::IsDebuggerPresent();
#elif defined(__linux__)
    // A process is being traced (debugged) if "/proc/self/status" contains a
    // line with "TracerPid: <non-zero-digit>...".
    std::ifstream fin("/proc/self/status");
    std::string line;
    while (!is_traced && std::getline(fin, line)) {
        const char kPrefix[] = "TracerPid:\t";
        static constexpr int kPrefixLen = sizeof(kPrefix) - 1;
        if (line.length() > kPrefixLen && line.compare(0, kPrefixLen, kPrefix) == 0) {
            if (line[kPrefixLen] != '0') {
                return true;
            }
        }
    }
    return false;
#elif defined(__APPLE__)
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
    kinfo_proc info = {};
    size_t size = sizeof(info);
    sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, nullptr, 0);
    return ((info.kp_proc.p_flag & P_TRACED) != 0);
#else
    return false;
#endif
}

void tint::debugger::Break() {
#ifdef _MSC_VER
    if (::IsDebuggerPresent()) {
        ::DebugBreak();
    }
#elif defined(__linux__)
    if (Attached()) {
        if (is_traced) {
            raise(SIGTRAP);
        }
    }
#elif defined(__APPLE__)
// TODO
#else
// unsupported platform
#endif
}

#else  // TINT_ENABLE_BREAK_IN_DEBUGGER

void tint::debugger::Break() {}
bool tint::debugger::Attached() {
    return false;
}

#endif  // TINT_ENABLE_BREAK_IN_DEBUGGER
