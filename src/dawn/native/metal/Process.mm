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

#include "dawn/native/metal/Process.h"

#include <spawn.h>
#include <stdlib.h>

#include "dawn/common/Assert.h"

namespace dawn::native::metal {
namespace {

bool MakePipe(ScopedFD& read, ScopedFD& write) {
    int pipeFds[2];
    int result = pipe(pipeFds);
    if (result == 0) {
        read.Reset(pipeFds[0]);
        write.Reset(pipeFds[1]);
        return true;
    }
    return false;
}

class PosixSpawnFileActions {
  public:
    PosixSpawnFileActions() { posix_spawn_file_actions_init(&mFileActions); }
    ~PosixSpawnFileActions() { posix_spawn_file_actions_destroy(&mFileActions); }
    posix_spawn_file_actions_t* Get() { return &mFileActions; }
    void AddDup2(int fd, int newFd) { posix_spawn_file_actions_adddup2(&mFileActions, fd, newFd); }
    void Inherit(int fd) { posix_spawn_file_actions_addinherit_np(&mFileActions, fd); }

  private:
    posix_spawn_file_actions_t mFileActions;
};

class PosixSpawnAttr {
  public:
    PosixSpawnAttr() { posix_spawnattr_init(&mAttr); }
    ~PosixSpawnAttr() { posix_spawnattr_destroy(&mAttr); }
    posix_spawnattr_t* Get() { return &mAttr; }
    void SetFlags(short flags) { posix_spawnattr_setflags(&mAttr, flags); }

  private:
    posix_spawnattr_t mAttr;
};

}  // namespace

// static
std::unique_ptr<Process> Process::MakeInternal(const std::vector<const char*>& args,
                                               ScopedFD pipeStdin[2],
                                               ScopedFD pipeStdout[2]) {
    int result = -1;
    PosixSpawnFileActions fileActions;

    // [0] is for read, [1] is for write
    fileActions.AddDup2(pipeStdin[0].Get(), STDIN_FILENO);
    fileActions.AddDup2(pipeStdout[1].Get(), STDOUT_FILENO);

    // Inherit stderr
    fileActions.Inherit(STDERR_FILENO);

    PosixSpawnAttr attr;
    attr.SetFlags(POSIX_SPAWN_CLOEXEC_DEFAULT);

    pid_t pid = -1;
    result = posix_spawnp(&pid, args[0], fileActions.Get(), attr.Get(),
                          const_cast<char* const*>(args.data()),
                          /*envp=*/nullptr);
    if (result != 0) {
        return {};
    }

    return std::unique_ptr<Process>(
        new Process(pid, std::move(pipeStdin[1]), std::move(pipeStdout[0])));
}

// static
std::unique_ptr<Process> Process::MakeWithStringInput(const std::vector<const char*>& args,
                                                      const std::string& input) {
    // Create pipes for stdin and stdout.
    ScopedFD pipeStdin[2];
    ScopedFD pipeStdout[2];

    MakePipe(pipeStdin[0], pipeStdin[1]);
    MakePipe(pipeStdout[0], pipeStdout[1]);

    auto process = MakeInternal(args, std::move(pipeStdin), std::move(pipeStdout));
    process->WriteToStdin(input);

    return process;
}

// static
std::unique_ptr<Process> Process::MakeWithProcessInput(const std::vector<const char*>& args,
                                                       std::unique_ptr<Process> input) {
    // Connect process's stdout to the new process stdin.
    ScopedFD pipeStdin[2] = {input->TakeStdout(), {}};

    // Create pipe for stdout.
    ScopedFD pipeStdout[2];
    MakePipe(pipeStdout[0], pipeStdout[1]);

    auto process = MakeInternal(args, std::move(pipeStdin), std::move(pipeStdout));

    auto stdin = input->TakeStdin();
    stdin.Reset();

    int exitCode;
    input->WaitForExit(exitCode);

    return process;
}

Process::Process(pid_t pid, ScopedFD stdinFd, ScopedFD stdoutFd)
    : mPid(pid), mStdin(std::move(stdinFd)), mStdout(std::move(stdoutFd)) {}

Process::~Process() {
    ASSERT(mPid == -1);
}

bool Process::WriteToStdin(const std::string& data) {
    ssize_t result = write(mStdin.Get(), data.data(), data.size());
    return result == static_cast<ssize_t>(data.size());
}

bool Process::CloseStdin() {
    mStdin.Reset();
    return true;
}

bool Process::ReadDataFromStdout(std::vector<char>& buffer, size_t size) {
    size_t offset = buffer.size();
    buffer.resize(offset + size);
    ssize_t result = read(mStdout.Get(), buffer.data() + offset, size);
    if (result < 0) {
        buffer.resize(offset);
        return false;
    }

    if (result == 0) {
        buffer.resize(offset);
        mStdout.Reset();
        int exitCode;
        WaitForExit(exitCode);
        return true;
    }

    buffer.resize(offset + result);
    return true;
}

bool Process::WaitForExit(int& exitCode) {
    ASSERT(mPid != -1);

    int status = 0;
    pid_t pid = waitpid(mPid, &status, 0);
    if (pid != mPid) {
        return false;
    }

    if (WIFSIGNALED(status)) {
        exitCode = -1;
    } else if (WIFEXITED(status)) {
        exitCode = WEXITSTATUS(status);
    }
    mPid = -1;
    return true;
}

}  // namespace dawn::native::metal
