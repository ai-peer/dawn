Dawn temporarily maintains a fork of the Emscripten WebGPU bindings
(`library_webgpu.js` and friends). The forked files live in
[`//third_party/emdawnwebgpu`](../third_party/emdawnwebgpu/)
and the build targets in this directory produce the other files needed to build
an Emscripten-based project using these bindings.

This allows the the webgpu.h interface to be kept in sync between the two
implementations in a single place (the Dawn repository) instead of two, while
also avoiding constantly breaking the version of webgpu.h that is currently in
Emscripten.

Changes to this code in the Dawn repository will be synced back out to the
upstream Emscripten repository after webgpu.h becomes stable, in what should
theoretically be one big final breaking update. Between then and now, projects
can use Dawn's fork of the bindings:

- Set up a GN build with `dawn_emscripten_dir` in the GN args set to point to
  a checkout of Emscripten. Note this must be a source checkout of Emscripten,
  not emsdk's `upstream/emscripten` release, which excludes necessary tools.
- Build the `emscripten_webgpu` GN build target.
- Configure the Emscripten build with `-sUSE_WEBGPU=0` (to disable the built-in
  bindings) and all of the linker flags listed in `emscripten_webgpu_config`.
