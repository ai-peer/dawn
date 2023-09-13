// Copyright 2023 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_METAL_PROCESS_H_
#define SRC_DAWN_NATIVE_METAL_PROCESS_H_

#include <memory>
#include <string>
#include <vector>

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

namespace dawn::native::metal {

class ScopedFD {
  public:
    ScopedFD() = default;
    ScopedFD(int fd) : mFd(fd) {}
    ScopedFD(ScopedFD&& fd) { this->operator=(std::move(fd)); }
    ~ScopedFD() { Reset(); }

    void Reset(int fd = -1) {
        if (mFd != -1) {
            close(mFd);
        }
        mFd = fd;
    }

    int Release() {
        int fd = mFd;
        mFd = -1;
        return fd;
    }

    int Get() const { return mFd; }

    const ScopedFD& operator=(int fd) {
        Reset();
        mFd = fd;
        return *this;
    }

    const ScopedFD& operator=(ScopedFD&& other) {
        Reset();
        mFd = other.mFd;
        other.mFd = -1;
        return *this;
    }

  private:
    int mFd = -1;
};

class Process {
  public:
    static std::unique_ptr<Process> MakeWithStringInput(const std::vector<const char*>& args,
                                                        const std::string& input);
    static std::unique_ptr<Process> MakeWithProcessInput(const std::vector<const char*>& args,
                                                         std::unique_ptr<Process> input);
    ~Process();

    bool WriteToStdin(const std::string& data);
    bool CloseStdin();
    bool ReadDataFromStdout(std::vector<char>& buffer, size_t size);
    bool CloseStdout();
    ScopedFD TakeStdin() { return std::move(mStdin); }
    ScopedFD TakeStdout() { return std::move(mStdout); }

    bool WaitForExit(int& exit_code);
    bool IsExited() const { return mPid == -1; }

  private:
    static std::unique_ptr<Process> MakeInternal(const std::vector<const char*>& args,
                                                 ScopedFD pipeStdin[2],
                                                 ScopedFD pipeStdout[2]);

    Process(pid_t pid, ScopedFD stdinFd, ScopedFD stdoutFd);

    pid_t mPid;
    ScopedFD mStdin;
    ScopedFD mStdout;
};

}  // namespace dawn::native::metal

#endif  // SRC_DAWN_NATIVE_METAL_PROCESS_H_
