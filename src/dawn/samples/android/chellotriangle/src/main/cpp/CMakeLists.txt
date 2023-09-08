cmake_minimum_required(VERSION 3.22.1)

project("chellotriangle")

#set(DAWN_FETCH_DEPENDENCIES ON)
set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u \
    Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")

add_library(chellotriangle MODULE
        CHelloTriangle.cpp
        RendererC.cc)

#find_package(game-activity REQUIRED CONFIG)

target_link_libraries(chellotriangle
        android-samples)