cmake_minimum_required(VERSION 3.22.1)

project("chellotriangle")

set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u \
    Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")

add_library(chellotriangle MODULE
        CHelloTriangle.cpp
        RendererC.cc)

target_link_libraries(chellotriangle
dawn_sample_utils)