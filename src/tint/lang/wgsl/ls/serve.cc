// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/tint/lang/wgsl/ls/serve.h"

#include <stdio.h>
#include <thread>

#include "langsvr/content_stream.h"
#include "langsvr/lsp/lsp.h"
#include "langsvr/session.h"

#include "src/tint/lang/wgsl/ls/server.h"
#include "src/tint/utils/macros/compiler.h"
#include "src/tint/utils/macros/defer.h"

#define WAIT_FOR_DEBUGGER 0

#if WAIT_FOR_DEBUGGER
#include <unistd.h>
#include <thread>
#endif

#define LOG(msg, ...)                          \
    {                                          \
        fprintf(log, msg "\n", ##__VA_ARGS__); \
        fflush(log);                           \
    }                                          \
    TINT_REQUIRE_SEMICOLON

namespace lsp = langsvr::lsp;

namespace tint::wgsl::ls {

namespace {

FILE* log = nullptr;

void TintInternalCompilerErrorReporter(const tint::InternalCompilerError& err) {
    if (log) {
        LOG("\n--------------------------------------------------------------");
        LOG("%s:%d %s", err.File(), static_cast<int>(err.Line()), err.Message().c_str());
        LOG("--------------------------------------------------------------\n");
    }
}

// TINT_BEGIN_DISABLE_WARNING(RESERVED_IDENTIFIER);
// extern "C" void __sanitizer_set_report_path(const char* path);
// TINT_END_DISABLE_WARNING(RESERVED_IDENTIFIER);

}  // namespace

Result<SuccessType> Serve(langsvr::Reader& reader, langsvr::Writer& writer) {
    log = fopen("log.txt", "wb");
    TINT_DEFER(fclose(log));

#if WAIT_FOR_DEBUGGER
    LOG("waiting for debugger. pid: %s", std::to_string(getpid()).c_str());
    std::this_thread::sleep_for(std::chrono::seconds(10));
#endif

    tint::SetInternalCompilerErrorReporter(&TintInternalCompilerErrorReporter);
    // __sanitizer_set_report_path("/home/ben/src/dawn/crash.txt");

    langsvr::Session session;
    session.SetSender([&](std::string_view response) {  //
        LOG("<< %s", std::string(response).c_str());
        return langsvr::WriteContent(writer, response);
    });

    Server server(session);

    LOG("Running...");

    while (!server.Shutdown()) {
        auto msg = langsvr::ReadContent(reader);
        if (msg != langsvr::Success) {
            LOG("ERROR: %s", msg.Failure().reason.c_str());
            break;
        }
        LOG(">> %s", msg.Get().c_str());

        auto res = session.Receive(msg.Get());
        if (res != langsvr::Success) {
            LOG("ERROR: %s", res.Failure().reason.c_str());
            break;
        }

        LOG("----------------");
    }

    LOG("Shutting down");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return Success;
}

}  // namespace tint::wgsl::ls
