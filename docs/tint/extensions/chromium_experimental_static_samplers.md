# Chromium Experimental Static Samplers

The chromium_experimental_static_samplers extension is an experimental extension that allows the use of static samplers in WGSL.
Static samplers translate to immutable samplers in Vulkan, constexpr samplers in Metal Shading Language, and static samplers in Direct3D 12.

# Status

This extension is experimental and the syntax is being discussed. No official WebGPU specification has been written yet.

# Pseudo-specification

This extension adds `sampler(...)` and `sampler_comparison(...)` functions to return a sampler type. 
The functions take arguments in the same order as `WGPUSamplerDescriptor`. If a sampler is initialized with any of the two functions, it is considered a static sampler, otherwise it is a normal sampler. 
The functions have overloads, to accomodate for the large number of parameters that can be set on a sampler, so that some can be defaulted.


Overloads for the constructors are:
- specifying addressModeW requires specifying address modes for u and v
- specifying addressModeV requires specifying address modes for u
- if only addressModeU if specified, it is used for all three dimensions
- if only magFilter is specified, it is used for minFilter as well
- lodMinClamp and lodMaxClamp are both specified, or neither

# Example usage

```wgsl
@group(0) @binding(0) var mySampler = sampler(mirror_repeat, linear, mipmap_linear)

@group(0) @binding(1) var myComparisonSampler = sampler_comparison(mirror_repeat, linear, mipmap_linear, less_equal)
```

