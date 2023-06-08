# Adapter Options (Unstable!)

`wgpu::RequestAdapterOptions` may be passed to `wgpuInstanceRequestAdapter` to request an adapter.

Dawn provides a few chained extension structs on `RequestAdapterOptions` to customize the behavior.
Currently, the `WGPUInstance` doesn't provide a way to query support for these features, so these
features may not be suitable for general use yet. Currently, they are used in Dawn's testing, and
in Chromium-specific integration with Dawn.

### `RequestAdapterOptionsBackendType`

Filters the adapters that Dawn discovers to only one particular backend.

### `RequestAdapterOptionsGetGLProc`

When discovering adapters on the GL backend, Dawn uses the provided `RequestAdapterOptionsGetGLProc::getProc` method to load GL procs. This extension struct does nothing on other backends.

### `RequestAdapterOptionsIDXGIAdapter`

When discovering adapters on the D3D11 and D3D12 backends, Dawn only discovers adapters matching the provided `RequestAdapterOptionsIDXGIAdapter::dxgiAdapter`. This extension struct does nothing on other backends.
