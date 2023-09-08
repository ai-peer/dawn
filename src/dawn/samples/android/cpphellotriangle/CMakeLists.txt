add_library(cpphellotriangle MODULE
  src/main/cpp/CppHelloTriangle.cpp
  src/main/cpp/Renderer.cc
  src/main/cpp/Renderer.h
)

target_link_options(cpphellotriangle PUBLIC
  "-u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode"
)

target_link_libraries(cpphellotriangle dawn_sample_utils)