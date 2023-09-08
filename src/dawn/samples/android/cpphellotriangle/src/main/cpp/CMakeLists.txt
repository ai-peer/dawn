# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.22.1)

project("cpphellotriangle")
#
#set(DAWN_FETCH_DEPENDENCIES ON)
set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u \
    Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")


add_library(cpphellotriangle MODULE
        HelloTriangleCpp.cpp
        Renderer.cc
        )

find_package(game-activity REQUIRED CONFIG)


target_link_libraries(cpphellotriangle
dawn_sample_utils
jnigraphics
game-activity::game-activity_static
android)