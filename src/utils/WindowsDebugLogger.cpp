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

#include "utils/WindowsDebugLogger.h"

#include "common/Assert.h"
#include "common/windows_with_undefs.h"

namespace utils {

    WindowsDebugLogger::WindowsDebugLogger() : mRunning(false) {
        if (IsDebuggerPresent()) {
            // This condition is true when running inside Visual Studio or some other debugger.
            // Messages are already printed there so we don't need to do anything.
            return;
        }

        mThread = std::thread(
            [](std::atomic_bool* running) {
                *running = true;

                struct {
                    DWORD process_id;
                    char data[4096 - sizeof(DWORD)];
                }* odsBuffer = nullptr;

                HANDLE file = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                                 sizeof(*odsBuffer), "DBWIN_BUFFER");
                ASSERT(file != INVALID_HANDLE_VALUE);

                odsBuffer = reinterpret_cast<decltype(odsBuffer)>(
                    MapViewOfFile(file, SECTION_MAP_READ, 0, 0, 0));
                ASSERT(odsBuffer);

                HANDLE odsBufferReady = CreateEventA(NULL, FALSE, FALSE, "DBWIN_BUFFER_READY");
                ASSERT(odsBufferReady);

                HANDLE odsDataReady = CreateEventA(NULL, FALSE, FALSE, "DBWIN_DATA_READY");
                ASSERT(odsDataReady);

                while (*running) {
                    SetEvent(odsBufferReady);
                    DWORD wait = WaitForSingleObject(odsDataReady, INFINITE);
                    ASSERT(wait == WAIT_OBJECT_0);
                    fprintf(stderr, "%.*s\n", static_cast<int>(sizeof(odsBuffer->data)),
                            odsBuffer->data);
                    fflush(stderr);
                }

                CloseHandle(odsDataReady);
                CloseHandle(odsBufferReady);
                UnmapViewOfFile(file);
                CloseHandle(file);
            },
            &mRunning);
    }

    WindowsDebugLogger::~WindowsDebugLogger() {
        if (mRunning && mThread.joinable()) {
            mRunning = false;
            mThread.join();
        }
    }

}  // namespace utils
