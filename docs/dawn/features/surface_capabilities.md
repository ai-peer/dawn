# Surface Capabilities

The `surface-capabilities` feature allows querying a surface's capabilities and creating a swap chain with additional usage flags.

Additional functionality:
 - Adds `wgpu::Surface::GetSupportedUsages(wgpu::Adapter)` method for querying the surface's supported usage flags. One or the combination of these flags can be used to create a swap chain.

Example Usage:
```
wgpu::TextureUsage supportedUsages = surface.GetSupportedUsages(adapter);

wgpu::SwapChainDescriptor desc = {};
// set usage flags.
desc.usage = supportedUsages;

device.CreateSwapChain(surface, &desc);
```

Notes:
 - If this feature is not enabled, only `wgpu::TextureUsage::RenderAttachment` flag is allowed to be used in `wgpu::SwapChainDescriptor::usage`.
