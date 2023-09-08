#include <jni.h>
#include <game-activity/GameActivity.cpp>
#include "RendererC.h"

extern "C" {
#include <game-activity/native_app_glue/android_native_app_glue.c>

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
        if (ALooper_pollAll(0, nullptr, &events, (void**)&pSource) >= 0) {
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
