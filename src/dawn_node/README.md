# Dawn bindings for NodeJS

Note: This code is currently WIP. There are a large number of [known issues](#known-issues).

## Building

## System requirements

- [CMake 3.10](https://cmake.org/download/) or greater

## Install `depot_tools`

Dawn uses the Chromium build system and dependency management so you need to [install depot_tools] and add it to the PATH.

[install depot_tools]: http://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up

### Fetch dependencies

Follow the `System requirements`, `Install depot_tools` steps of [`doc/building.md`](docs/building.md).

Get the code, along with dependencies for NodeJS:

```sh
# Clone the repo as "dawn"
git clone https://dawn.googlesource.com/dawn dawn && cd dawn

# Bootstrap the NodeJS binding gclient configuration
cp scripts/standalone-with-node.gclient .gclient

# Fetch external dependencies and toolchains with gclient
gclient sync
```

### Build

```sh
mkdir <build-output-path>
cd <build-output-path>
cmake <dawn-root-path> -GNinja -DDAWN_BUILD_NODE_BINDINGS
ninja
```

## Known issues

- Many WebGPU CTS tests are currently known to fail
- Windows and macOS builds are presumed broken
- Dawn uses special token values for some parameters / fields. These are currently passed straight through to dawn from the JavaScript. discussions: [1](https://dawn-review.googlesource.com/c/dawn/+/64907/5/src/dawn_node/binding/Converter.cpp#167), [2](https://dawn-review.googlesource.com/c/dawn/+/64907/5/src/dawn_node/binding/Converter.cpp#928), [3](https://dawn-review.googlesource.com/c/dawn/+/64909/4/src/dawn_node/binding/GPUTexture.cpp#42)
- Backend validation is currently always set to 'full' to aid in debugging. This can be extremely slow. [discussion](https://dawn-review.googlesource.com/c/dawn/+/64916/4/src/dawn_node/binding/GPU.cpp#25)
- Attempting to call `new T` in JavaScript, where `T` is an IDL interface type, should result in a TypeError "Illegal constructor". [discussion](https://dawn-review.googlesource.com/c/dawn/+/64902/9/src/dawn_node/interop/WebGPU.cpp.tmpl#293)
- `GPUDevice` currently maintains a list of "lost promises". This should return the same promise. [discussion](https://dawn-review.googlesource.com/c/dawn/+/64906/6/src/dawn_node/binding/GPUDevice.h#107)

## Remaining work

- Have the IDL interop generator produce default values for parameters (instead of std::optional). [discussion](https://dawn-review.googlesource.com/c/dawn/+/64912/5/src/dawn_node/binding/GPURenderBundleEncoder.cpp#166)
- Investigate CTS failures that are not expected to fail.
- Fix Windows and macOS builds.
- Generated includes live in `src/` for `dawn_node`, but outside for Dawn. [discussion](https://dawn-review.googlesource.com/c/dawn/+/64903/9/src/dawn_node/interop/CMakeLists.txt#56)
- Hook up to presubmit bots (CQ / Kokoro)
- `binding::GPU` will require significant rework [once Dawn implements the device / adapter creation path properly](https://dawn-review.googlesource.com/c/dawn/+/64916/4/src/dawn_node/binding/GPU.cpp).
