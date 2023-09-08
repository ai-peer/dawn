cmake_minimum_required(VERSION 3.22.1)

project("cpphellotriangle")

set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u \
    Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")

add_library(cpphellotriangle MODULE
        CppHelloTriangle.cpp
        Renderer.cc)

target_link_libraries(cpphellotriangle
dawn_sample_utils)