{
  "targets": [
    {
      "target_name": "dawn",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "include_dirs": [
        "../examples",
        "../out/Debug/gen",
        "../src",
        "../src/include",
        "../third_party/glfw/include",
        "<!@(node -p \"require('node-addon-api').include\")",
      ],
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS",
        "DAWN_ENABLE_ASSERTS",
        "DAWN_ENABLE_BACKEND_NULL",
        "DAWN_ENABLE_BACKEND_OPENGL",
        "DAWN_ENABLE_BACKEND_VULKAN",
      ],
      "sources": [
        "dawn.cpp",
        "../examples/SampleUtils.h",
        "../examples/SampleUtils.cpp",
        "../src/utils/BackendBinding.h",
        "../src/utils/BackendBinding.cpp",
        "../src/utils/NullBinding.cpp",
      ],
      "libraries": [
        "<!(pwd)/../out/Debug/libdawn_sample.so",
        "<!(pwd)/../out/Debug/obj/libdawn_utils.a",
        "<!(pwd)/../out/Debug/obj/third_party/libglfw.a",
        "<!(pwd)/../out/Debug/obj/third_party/libglad.a",
      ],
    }
  ]
}
