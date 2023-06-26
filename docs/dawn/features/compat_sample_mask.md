# Compat Sample Mask

The `compat-sample-mask` feature allows usage of sample_mask builtins in shaders with
OpenGL compat backend with the GL_OES_sample_variables extension.

This is now a dawn specific feature, before WebGPU working group feature name is finalized.

Example Usage:
```
// Simply use @builtin(sample_mask) in wgsl when device has this feature.
struct Output {
    @builtin(sample_mask) mask_out: u32,
    @location(0) color : vec4f,
}
```
