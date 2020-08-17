use_relative_paths = True

gclient_gn_args_file = 'build/config/gclient_args.gni'
gclient_gn_args = [
  'mac_xcode_version',
]

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
  'dawn_git': 'https://dawn.googlesource.com',
  'github_git': 'https://github.com',
  'swiftshader_git': 'https://swiftshader.googlesource.com',

  'dawn_standalone': True,

  # This can be overridden, e.g. with custom_vars, to download a nonstandard
  # Xcode version in build/mac_toolchain.py instead of downloading the
  # prebuilt pinned revision.
  'mac_xcode_version': 'default',
}

deps = {
  # Dependencies required to use GN/Clang in standalone
  'build': {
    'url': '{chromium_git}/chromium/src/build@b8f14c09b76ae3bd6edabe45105527a97e1e16bd',
    'condition': 'dawn_standalone',
  },
  'buildtools': {
    'url': '{chromium_git}/chromium/src/buildtools@eb3987ec709b39469423100c1e77f0446890e059',
    'condition': 'dawn_standalone',
  },
  'tools/clang': {
    'url': '{chromium_git}/chromium/src/tools/clang@d027d75e8dd91140115a4cc9c7c3598c44bbf634',
    'condition': 'dawn_standalone',
  },
  'tools/clang/dsymutil': {
    'packages': [
      {
        'package': 'chromium/llvm-build-tools/dsymutil',
        'version': 'M56jPzDv1620Rnm__jTMYS62Zi8rxHVq7yw0qeBFEgkC',
      }
    ],
    'condition': 'checkout_mac or checkout_ios',
    'dep_type': 'cipd',
  },

  # Testing, GTest and GMock
  'testing': {
    'url': '{chromium_git}/chromium/src/testing@e5ced5141379ee8ae28b4f93d3c02df039d2b052',
    'condition': 'dawn_standalone',
  },
  'third_party/googletest': {
    'url': '{chromium_git}/external/github.com/google/googletest@a09ea700d32bab83325aff9ff34d0582e50e3997',
    'condition': 'dawn_standalone',
  },

  # Jinja2 and MarkupSafe for the code generator
  'third_party/jinja2': {
    'url': '{chromium_git}/chromium/src/third_party/jinja2@b41863e42637544c2941b574c7877d3e1f663e25',
    'condition': 'dawn_standalone',
  },
  'third_party/markupsafe': {
    'url': '{chromium_git}/chromium/src/third_party/markupsafe@8f45f5cfa0009d2a70589bcda0349b8cb2b72783',
    'condition': 'dawn_standalone',
  },

  # SPIRV-Cross
  'third_party/spirv-cross': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Cross@4c7944bb4260ab0466817c932a9673b6cf59438e',
    'condition': 'dawn_standalone',
  },

  # SPIRV compiler dependencies: SPIRV-Tools, SPIRV-headers, glslang and shaderc
  'third_party/SPIRV-Tools': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Tools@1023dd7a04be15064188d0e511e1708ef1c5af4a',
    'condition': 'dawn_standalone',
  },
  'third_party/spirv-headers': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Headers@3fdabd0da2932c276b25b9b4a988ba134eba1aa6',
    'condition': 'dawn_standalone',
  },
  'third_party/glslang': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/glslang@f257e0ea6b9aeab2dc7af3207ac6d29d2bbc01d0',
    'condition': 'dawn_standalone',
  },
  'third_party/shaderc': {
    'url': '{chromium_git}/external/github.com/google/shaderc@21b36f7368092216ecfaa017e95c383c2ed9db70',
    'condition': 'dawn_standalone',
  },

  # WGSL support
  'third_party/tint': {
    'url': '{dawn_git}/tint@b08e25388629f5a8f467b726ace18df529bba3ef',
    'condition': 'dawn_standalone',
  },

  # GLFW for tests and samples
  'third_party/glfw': {
    'url': '{chromium_git}/external/github.com/glfw/glfw@d973acc123826666ecc9e6fd475682e3d84c54a6',
    'condition': 'dawn_standalone',
  },

  # Dependencies for samples: GLM
  'third_party/glm': {
    'url': '{github_git}/g-truc/glm.git@bf71a834948186f4097caa076cd2663c69a10e1e',
    'condition': 'dawn_standalone',
  },

  # Khronos Vulkan headers, validation layers and loader.
  'third_party/vulkan-headers': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Headers@4c079bf40c2587220dbf157d825d3185c9adc896',
    'condition': 'dawn_standalone',
  },
  'third_party/vulkan-validation-layers': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-ValidationLayers@e8b96e86fe2edfaee274b98fbbe1bd65579b0904',
    'condition': 'dawn_standalone',
  },
  'third_party/vulkan-loader': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Loader@006586926adece57adea3e006140b5df19826371',
    'condition': 'dawn_standalone',
  },

  'third_party/swiftshader': {
    'url': '{swiftshader_git}/SwiftShader@e8dd233c7a85f3c689caf06c226a7f8405a480d3',
    'condition': 'dawn_standalone',
  },

}

hooks = [
  # Pull the compilers and system libraries for hermetic builds
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'checkout_linux and ((checkout_x86 or checkout_x64) and dawn_standalone)',
    'action': ['python', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x86'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and (checkout_x64 and dawn_standalone)',
    'action': ['python', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x64'],
  },
  {
    # Update the Windows toolchain if necessary. Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win and dawn_standalone',
    'action': ['python', 'build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python', 'tools/clang/scripts/update.py'],
    'condition': 'dawn_standalone',
  },
  {
    # Pull rc binaries using checked-in hashes.
    'name': 'rc_win',
    'pattern': '.',
    'condition': 'checkout_win and (host_os == "win" and dawn_standalone)',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'build/toolchain/win/rc/win/rc.exe.sha1',
    ],
  },
  # Update build/util/LASTCHANGE.
  {
    'name': 'lastchange',
    'pattern': '.',
    'condition': 'dawn_standalone',
    'action': ['python', 'build/util/lastchange.py',
               '-o', 'build/util/LASTCHANGE'],
  },
]

recursedeps = [
  # buildtools provides clang_format, libc++, and libc++abi
  'buildtools',
]
