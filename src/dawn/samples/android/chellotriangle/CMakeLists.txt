add_library(chellotriangle MODULE
  src/main/cpp/CHelloTriangle.cpp
  src/main/cpp/RendererC.cc
  src/main/cpp/RendererC.h
)

target_link_options(chellotriangle PUBLIC
  "-u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode"
)

target_link_libraries(chellotriangle dawn_sample_utils)