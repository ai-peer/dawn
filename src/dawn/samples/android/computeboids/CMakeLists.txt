add_library(computeboids MODULE
  src/main/cpp/Compute.cpp
  src/main/cpp/Renderer.cc
  src/main/cpp/Renderer.h
)

target_link_options(computeboids PUBLIC
  "-u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode"
)

target_link_libraries(computeboids dawn_sample_utils)