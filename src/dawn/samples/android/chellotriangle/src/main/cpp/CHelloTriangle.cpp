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
#include <game-activity/GameActivity.cpp>
#include "dawn/samples/android/chelloTriangle/main/cpp/RendererC.h"

extern "C" {
#include <game-activity/native_app_glue/android_native_app_glue.c>  // NOLINT

void handle_cmd(android_app* pApp, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            // A new window is created, associate a renderer with it.
            pApp->userData = new RendererC(pApp);
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being destroyed. Use this to clean up your userData to avoid leaking
            // resources.
            if (pApp->userData) {
                auto* pRenderer = reinterpret_cast<RendererC*>(pApp->userData);
                pApp->userData = nullptr;
                delete pRenderer;
            }
            break;
        default:
            break;
    }
}

void android_main(android_app* pApp) {
    pApp->onAppCmd = handle_cmd;
    int events;
    android_poll_source* pSource;
    do {
        if (ALooper_pollAll(0, nullptr, &events, reinterpret_cast<void**>(&pSource)) >= 0) {
            if (pSource) {
                pSource->process(pApp, pSource);
            }
        }
        if (pApp->userData) {
            auto* pRenderer = reinterpret_cast<RendererC*>(pApp->userData);
            pRenderer->GameLoop();
        }
    } while (!pApp->destroyRequested);
}
}
