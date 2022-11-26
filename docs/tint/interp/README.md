# WGSL interpreter and shader debugger

Tint contains a library that provides support for running WGSL shaders in an
interpreter. This can be used to interactively debug WGSL shaders, and has the
capability to detect various potential problems during dynamic execution of the
shader.

This is coupled to an "Emulator" backend in Dawn, which allows applications that
use the WebGPU API to run on the interpreter.

For design details, see [arch.md](arch.md).

## Limitations

The interpreter is currently limited to compute shaders.
There are no immediate plans to add support for render pipelines, though this
may be supported in the future.

The following features are currently unsupported, but will likely be implemented
in the future:
- Textures and samplers

As it is interpreting a high-level language, the interpreter runs shaders
relatively slowly compared to running on real hardware. Consider trying to run
a smaller dispatch when debugging is the performance is prohibitive.

## Building

Follow instructions in [../../building.md](../../building.md), with the
following modifications.

For GN, add `tint_build_interpreter=true` and `dawn_enable_emulator=true` to the gn args file.

For CMake, add `-DTINT_BUILD_INTERPRETER=ON -DDAWN_ENABLE_EMULATOR=ON` to the
command line flags.

## Usage

The Dawn Emulator backend appears to applications as another WebGPU adapter, and
can be targeted by selecting that adapter and creating a device from it. By
default the emulator will run non-interactively and report any potential
problems it finds to stderr.

There are two Dawn toggles to enable specific debugging features:
- `interactive` - enable the interactive debugger CLI (uses stdin/stdout)
- `enable_drd` - enable a data race detector

### Debugging shaders in WebGPU CTS via dawn.node

See [the dawn.node README](../../../src/dawn/node/README.md) for details on how
to build and use dawn.node for running CTS tests.

To enable the Emulator backend, pass `--backend=emulator` to the `run-cts` tool.
To enable interactive debugging, pass `--interactive`.
To enable the data race detector, pass `--flag=enable-dawn-features=enable_drd`.

### Debugging shaders in Dawn E2E tests

The tests need to be enabled for the emulator by adding `EmulatorBackend()` to
the list of backends in the corresponding `DAWN_INSTANTIATE_TEST` invocation.

To enable the Emulator backend, pass `--backend=emulator`.
To enable interactive debugging, pass `--enable-toggles=interactive`.
To enable the data race detector, pass `--enable-toggles=enable_drd`.

### Debugging shaders when linking against Dawn Native

TODO(jrprice): Add instructions for using the emulator with Dawn Native.
