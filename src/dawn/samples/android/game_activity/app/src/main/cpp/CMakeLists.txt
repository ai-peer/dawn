cmake_minimum_required(VERSION 3.22.1)

project("game_activity")

set(DAWN_FETCH_DEPENDENCIES ON)
set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u \
    Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")

add_library(game_activity MODULE
        WGPUHelloTriangle.cpp
        WGPUAndroidAppUtils.cpp
        WGPUAndroidAppRenderer.cpp)

find_package(game-activity REQUIRED CONFIG)

target_link_libraries(game_activity
        game-activity::game-activity
        jnigraphics
        game-activity::game-activity_static
        android
        log
        webgpu_dawn webgpu_cpp)