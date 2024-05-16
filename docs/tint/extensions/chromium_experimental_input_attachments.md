# Chromium Experimental Input Attachments

The `chromium_experimental_input_attachments` extension adds support for input attachment global variables to WGSL.
This is similar to Vulkan's `subpassInput` variable.

## Status

Input attachments support in Tint is highly experimental and only meant to be used in internal transforms at this stage.
This extension is only relevant to SPIRV backend atm.
Specification work in the WebGPU group hasn't started.

## Pseudo-specification

This extension adds:
- A new `input_attachment` type that's only allowed on global variable declarations.
- A new `input_attachment_index` attribute. Unlike `binding` which specifies the binding point in a BindGroup, this attribute specifies the index of the input attachment in the render pass descriptor.
- A new `inputAttachmentLoad` builtin function.

## Example usage

```
@group(0) @binding(0) @input_attachment_index(0)
    var input_tex : input_attachment<f32>;

@fragment fn main() -> @location(0) vec4f {
    return inputAttachmentLoad(input_tex);
}
```
