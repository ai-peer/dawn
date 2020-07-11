// Copyright 2020 The Dawn Authors
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

#ifndef UTILS_WINDOWSDEBUGLOGGER_H_
#define UTILS_WINDOWSDEBUGLOGGER_H_

#include <atomic>
#include <thread>

namespace utils {

    class WindowsDebugLogger {
      public:
        WindowsDebugLogger();
        ~WindowsDebugLogger();

        WindowsDebugLogger(const WindowsDebugLogger&) = delete;
        WindowsDebugLogger& operator=(const WindowsDebugLogger&) = delete;
        WindowsDebugLogger(WindowsDebugLogger&&) = delete;
        WindowsDebugLogger& operator=(WindowsDebugLogger&&) = delete;

      private:
        std::atomic_bool mRunning;
        std::thread mThread;
    };

}  // namespace utils

#endif  // UTILS_WINDOWSDEBUGLOGGER_H_
