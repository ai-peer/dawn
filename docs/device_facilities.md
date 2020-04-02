# Devices

In Dawn the `Device` is a "god object" that contains a lot of facilities useful for the whole object graph that descends from it.
There a number of facilities common to all backends that live in the frontend and backend-specific facilities.
Example of frontend facilities are the management of content-less object caches, or the toggle management.
Example of backend facilities are GPU memory allocators or the backing API function pointer table.

## Frontend facilities

### Error Handling

Dawn (dawn_native) uses the [Error.h](../src/dawn_native/Error.h) error handling to robustly handle errors.
With `DAWN_TRY` errors bubble up all the way to, and are "consumed" by the entry-point that was called by the application.
Error consumption uses `Device::ConsumeError` that expose them via the WebGPU "error scopes" and can also influence the device lifecycle.

See [Error.h](../src/dawn_native/Error.h) for more information about using errors.

### Device Lifecycle



### Toggles

Toggles are booleans that control code paths inside of Dawn, like lazy-clearing resources or using D3D12 render passes.
They aren't just booleans close to the code path they control, because embedders of Dawn like Chromium want to be able to surface what toggles are used by a device (like in about:gpu).

Toogles are to be used for any optional code path in Dawn, including:

 - Workarounds for driver bugs.
 - Disabling select parts of the validation or robustness.
 - Enabling limitations that help with testing.
 - Using more advanced or optional API features.

Toggles can be queried using `DeviceBase::IsToggleEnabled`:
```
bool useRenderPass = device->IsToggleEnabled(Toggle::UseD3D12RenderPass);
```

Toggles are defined in a table in [Toggles.cpp](../src/dawn_native/Toggles.cpp) that also includes their name and description.
The name can be used to force enabling of a toggle or, at the contrary, force the disabling of a toogle.
This is particularly useful in tests so that the two sides of a code path can be tested (for example using D3D12 render passes and not).

Here's an example of a test that is run in the D3D12 backend both with the D3D12 render passes forcibly disabled, and in the default configuration.
```
DAWN_INSTANTIATE_TEST(RenderPassTest,
                      D3D12Backend(),
                      D3D12Backend({}, {"use_d3d12_render_pass"}));
// The {} is the list of force enabled toggles, {"..."} the force disabled ones.
```

The initialization order of toggles looks as follows:

 - The toggles overrides from the device descriptor are applied.
 - The frontend device default toggles are applied (unless already overriden).
 - The backend device default toggles are applied (unless already overriden) using `DeviceBase::SetToggle`
 - The backend device can ignore overriden toggles if it can't support them by using `DeviceBase::ForceSetToggle`

### Immutable object caches

A number of WebGPU objects are immutable once created, and can be expensive to create, like pipelines.
`DeviceBase` contains caches for these objects so that they are free to create the second time.
This is also useful to be able to compare objects by pointers like `BindGroupLayouts` since two BGLs would be equal iff they are the same object.

### Format Tables

The frontend has a `Format` structure that represent all the information that are known about a particular WebGPU format for this Device based on the enabled extensions.
Formats are precomputed at device initialization and can be queried from a WebGPU format either assuming the format is a valid enum, or in a safe manner that doesn't do this assumption.
A reference to these formats can be stored persistently as they have the same lifetime as the `Device`.

Formats also have an "index" so that backends can create parallel tables for internal informations about formats, like what they translate to in the backing API.

### Object factory

Like WebGPU's device object, `DeviceBase` is an factory with methods to create all kinds of other WebGPU objects.
WebGPU has some objects that aren't created from the device, like the texture view, but in Dawn these creations also go through `DeviceBase` so that there is a single factory for each backend.
