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

#ifndef COMMON_LOG_H_
#define COMMON_LOG_H_

#include <sstream>

enum class LogSeverity {
    Debug,
    Info,
    Warning,
    Error,
};

class LogMessage {
  public:
    LogMessage(LogSeverity severity);
    ~LogMessage();

    LogMessage(LogMessage&& other) = default;
    LogMessage& operator=(LogMessage&& other) = default;

    template <typename T>
    LogMessage& operator<<(T&& value) {
        mStream << value;
        return *this;
    }

  private:
    LogMessage(const LogMessage& other) = delete;
    LogMessage& operator=(const LogMessage& other) = delete;

    LogSeverity mSeverity;
    std::ostringstream mStream;
};

LogMessage DebugLog();
LogMessage InfoLog();
LogMessage WarningLog();
LogMessage ErrorLog();

LogMessage DebugLog(const char* file, const char* function, int line);
#define DAWN_DEBUG() DebugLog(__FILE__, __func__, __LINE__)

#endif  // COMMON_LOG_H_
