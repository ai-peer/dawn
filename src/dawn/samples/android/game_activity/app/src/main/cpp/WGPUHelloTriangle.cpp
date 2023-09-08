
// Coded to open a window and color it using WGPU c++

#include <dawn/webgpu.h>
#include <iostream>

#include <android/log.h>
#include <jni.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <game-activity/GameActivity.cpp>
#include "WGPUAndroidAppRenderer.cpp"


extern "C" {
#include <game-activity/native_app_glue/android_native_app_glue.c>


/*!
 * Handles commands sent to this Android application
 * @param pApp the app the commands are coming from
 * @param cmd the command to handle
 */
void handle_cmd(android_app* pApp, int32_t cmd) {
  switch (cmd) {
    case APP_CMD_INIT_WINDOW:
      // A new window is created, associate a renderer with it. You may replace this with a
      // "game" class if that suits your needs. Remember to change all instances of userData
      // if you change the class here as a reinterpret_cast is dangerous this in the
      // android_main function and the APP_CMD_TERM_WINDOW handler case.
      pApp->userData = new WGPUAndroidAppRenderer(pApp);
//      pApp->userData = new firstColor(pApp);
      break;
    case APP_CMD_TERM_WINDOW:
      // The window is being destroyed. Use this to clean up your userData to avoid leaking
      // resources.
      //
      // We have to check if userData is assigned just in case this comes in really quickly
      if (pApp->userData) {
        //
        auto* pRenderer = reinterpret_cast<WGPUAndroidAppRenderer*>(pApp->userData);
//        auto* pRenderer = reinterpret_cast<firstColor*>(pApp->userData);
        pApp->userData = nullptr;
        delete pRenderer;
      }
      break;
    default:break;
  }
}

void android_main(android_app* pApp) {
  LOGI("Welcome to android_main");
//
//  WGPUAndroidAppRenderer* wgpu_android_app_renderer = new WGPUAndroidAppRenderer(pApp);
//  wgpu_android_app_renderer->GameLoop();
//  delete wgpu_android_app_renderer;

  // Register an event handler for Android events
  pApp->onAppCmd = handle_cmd;

  // This sets up a typical game/event loop. It will run until the app is destroyed.
  int events;
  android_poll_source *pSource;
  do {
    // Process all pending events before running game logic.
    if (ALooper_pollAll(0, nullptr, &events, (void **) &pSource) >= 0) {
      if (pSource) {
        pSource->process(pApp, pSource);
      }
    }

    // Check if any user data is associated. This is assigned in handle_cmd
    if (pApp->userData) {
      auto *pRenderer = reinterpret_cast<WGPUAndroidAppRenderer *>(pApp->userData);
//      auto *pRenderer = reinterpret_cast<firstColor *>(pApp->userData);

//      // Process game input
//      pRenderer->handleInput();

      pRenderer->GameLoop();
    }
  } while (!pApp->destroyRequested);

}

}
