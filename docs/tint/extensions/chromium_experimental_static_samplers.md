# Chromium Experimental Static Samplers

The chromium_experimental_static_samplers extension is an experimental extension that allows the use of static samplers in WGSL.
Static samplers translate to immutable samplers in Vulkan, constexpr samplers in Metal Shading Language, and static samplers in Direct3D 12.

# Status

This extension is experimental and the syntax is being discussed. No official WebGPU specification has been written yet.

# Design considerations

## const or var

`@group(X) @binding(Y) const mySampler;`
or
`@group(X) @binding(Y) var mySampler;`


In HLSL and SPIR-V, static samplers need a binding slot, hence the `@group(X) @binding(Y)` syntax.
Vulkan and Direct3D 12 supply static samplers via the API.
Metal Shading Language declares static samplers with `constexpr`, which is ready to be used in the shader without Metal API interaction.

Normal samplers in WGSL are declared with `var` because they have a handle address space, which is read-only.
Using the `const` keyword for static samplers is more intuitive, as static samplers are immutable objects.
However, `const` does not have a handle address space which is required for HLSL and SPIR-V and would require spec changes to support it.
When using `const`, the sampler would be immutable and copying it would alias the original sampler.

Using `var` for static samplers is recommended for consistency with the rest of the language and they are already immutable.

## Using a struct

```wgsl
@group(X) @binding(Y) const mySampler = sampler {
  addressModeU: mirror_repeat, // enumerant
  magFilter: linear,
  lodMinClamp: 1, // abstract float
  ...
};
```
The `sampler` is a built-in struct that can be used to define samplers.
Advantages are its readability and similarity to the WebGPU API's `GPUSamplerDescriptor` struct.

This approach would require a new WGSL feature for initializing struct values.

If the new WGSL feature is introduced, this approach is recommended for readability and intuitiveness.
However, for the initial implementation, it is not desirable to introduce a substantial change in WGSL.

## Using a function

```wgsl
sampler(mirror_repeat, linear, 0.0, 16);
sampler_comparison(mirror_repeat, linear, 0.0, 16);
```

or

```wgsl
sampler<mirror_repeat, linear>(0.0, 16);
sampler_comparison<mirror_repeat, linear>(0.0, 16);
```

In both cases, `sampler` is a built-in function that returns a static sampler.
The first approach takes a list of arguments.
The second approach takes a list of enum template parameters and a list of arguments.
In the second approach, the arguments are LOD min and max clamps, because they are not enums.
The first approach requires enum values to be passed as arguments which is not allowed in WGSL because of function restrictions.
The second approach does not require any changes to the language.

Both approaches are readable and intuitive but due to the large amount of parameters that can be set on a sampler, the approaches may become hard to read and write.

The recommended approach is to use a function with template parameters, because it is consistent with the rest of the language.
Neither approaches have a substantial advantage over the other.

# Pseudo-specification

This extension adds `sampler<...>(...)` and `sampler_comparison<...>(...)` functions to return a sampler type.
The functions take template parameters that are identical to the fields in `GPUSamplerDescriptor`. If a sampler is initialized with any of the two functions, it is considered a static sampler, otherwise it is a normal sampler.
The functions have overloads, to accomodate for the large number of parameters that can be set on a sampler, so that some can be defaulted.


Order of template parameters:
- addressModeU (required)
- addressModeV
- addressModeW
- magFilter (required)
- minFilter
- mipmapFilter (required)
- compare (only for sampler_comparison)

Order of arguments:
- lodMinClamp
- lodMaxClamp

Overloads for the functions:
- specifying addressModeW requires specifying address modes for u and v
- specifying addressModeV requires specifying address modes for u
- if only addressModeU if specified, it is used for all three dimensions
- if only magFilter is specified, it is used for minFilter as well
- lodMinClamp and lodMaxClamp are both specified, or neither

New Enums:
- Address Mode:
  - WGSL: `clamp_to_edge`, `repeat`, `mirror_repeat`
  - WebGPU eqivalent: `"clamp-to-edge"`, `"repeat"`, `"mirror-repeat"`
- Filter Mode:
  - WGSL: `nearest`, `linear`
  - WebGPU equivalent: `"nearest"`, `"linear"`
- Mipmap Filter Mode:
  - WGSL: `mipmap_nearest`, `mipmap_linear`
  - WebGPU equivalent: `"nearest"`, `"linear"`
- Comparison Function:
  - WGSL: `never`, `less`, `equal`, `less_equal`, `greater`, `not_equal`, `greater_equal`, `always`
  - WebGPU equivalent: `"never"`, `"less"`, `"equal"`, `"less-equal"`, `"greater"`, `"not-equal"`, `"greater-equal"`, `"always"`

# Example usage

```wgsl
@group(0) @binding(0) var mySampler = sampler<mirror_repeat, linear, mipmap_linear>(0.0, 16.0)

@group(0) @binding(1) var myComparisonSampler = sampler_comparison<mirror_repeat, linear, mipmap_linear, less_equal>(0.0, 16.0)

```

