# Adapter Options (Unstable!)

`wgpu::RequestAdapterOptions` may be passed to `wgpuInstanceRequestAdapter` to request an adapter.

Dawn provides a few chained extension structs on `RequestAdapterOptions` to customize the behavior.
Currently, the `WGPUInstance` doesn't provide a way to query support for these features, so these
features may not be suitable for general use yet. Currently, they are used in Dawn's testing, and
in Chromium-specific integration with Dawn.

`dawn::native::Instance::EnumerateAdapters` is a Dawn native-only API that may be used to synchronously
get a list of adapters according to the RequestAdapterOptions. The members are treated as follows:
 - `RequestAdapterOptions::compatibleSurface` is ignored.
 - `RequestAdapterOptions::powerPreference` adapters are sorted according to powerPreference such that
   preferred adapters are at the front of the list. It is a preference - so if
  wgpu::PowerPreference::LowPower is passed, the list may contain only integrated GPUs, a mix of integrated
  and discrete GPUs, or even only discrete GPUs.
 - `RequestAdapterOptions::compatibilityMode` all returned adapters must match the requested compatibiltiy mode.
 - `RequestAdapterOptions::forceFallbackAdapter` all returned adapters must be fallback adapters.

If no options are passed to EnumerateAdapters, then all discoverable adapters are returned. This includes both
compatibility mode adapters and core adapters.

### `RequestAdapterOptionsBackendType`

Filters the adapters that Dawn discovers to only one particular backend.

### `RequestAdapterOptionsGetGLProc`

When discovering adapters on the GL backend, Dawn uses the provided `RequestAdapterOptionsGetGLProc::getProc` method to load GL procs. This extension struct does nothing on other backends.

### `RequestAdapterOptionsIDXGIAdapter`

When discovering adapters on the D3D11 and D3D12 backends, Dawn only discovers adapters matching the provided `RequestAdapterOptionsIDXGIAdapter::dxgiAdapter`. This extension struct does nothing on other backends.
