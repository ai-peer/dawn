# WGSL interpreter and shader debugger

Tint contains a library that provides support for running WGSL shaders in an
interpreter. This can be used to interactively debug WGSL shaders, and has the
capability to detect various potential problems during dynamic execution of the
shader.

This is coupled to a "WGSL interpreter" backend in Dawn, which allows
applications that use the WebGPU API to run on the interpreter.

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
a smaller dispatch when debugging if the performance is prohibitive.

## Building

Follow instructions in [../../building.md](../../building.md), with the
following modifications.

For GN builds, add `tint_build_interpreter=true` and
`dawn_enable_wgsl_interpreter=true` to the gn args file.

For CMake, add `-DTINT_BUILD_INTERPRETER=ON -DDAWN_ENABLE_WGSL_INTERPRETER=ON`
to the command line flags.

## Usage

The Dawn WGSL interpter backend appears to applications as another WebGPU
adapter, and can be targeted by selecting that adapter and creating a device
from it. By default the interpreter will run non-interactively and report any
potential problems it finds to stderr.

There are two Dawn toggles to enable specific debugging features:
- `wgsl_interpreter_interactive` - enable the interactive debugger (uses stdout)
- `wgsl_interpreter_enable_drd` - enable the data race detector

### Debugging shaders in WebGPU CTS via dawn.node

See [the dawn.node README](../../../src/dawn/node/README.md) for details on how
to build and use dawn.node for running CTS tests.

* To use the WGSL interpreter backend, pass `--backend=interpreter` to the
  `run-cts` tool.
* To enable interactive debugging, pass `--debug`.
* To enable the data race detector, pass
  `--flag=enable-dawn-features=wgsl_interpreter_enable_drd`.

### Debugging shaders in Dawn E2E tests

The tests need to be enabled for the interpreter by adding
`InterpreterBackend()` to the list of backends in the corresponding
`DAWN_INSTANTIATE_TEST` invocation.

* To use the WGSL interpreter backend, pass `--backend=interpreter`.
* To enable interactive debugging, pass
  `--enable-toggles=wgsl_interpreter_interactive`.
* To enable the data race detector, pass
  `--enable-toggles=wgsl_interpreter_enable_drd`.

### Debugging shaders when linking against Dawn Native

TODO(jrprice): Add instructions for using the interpreter with Dawn Native.
