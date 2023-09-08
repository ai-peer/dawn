project(android-samples)
cmake_minimum_required(VERSION 3.22)

add_subdirectory(chellotriangle/src/main/cpp)
# add_subdirectory(cpphellotriangle/src/main/cpp)

add_library(android-samples SHARED temp.cpp)

find_package(game-activity REQUIRED CONFIG)

target_link_libraries(android-samples
        jnigraphics
        game-activity::game-activity_static
        android
        webgpu_dawn webgpu_cpp)
