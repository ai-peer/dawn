# Devices

In Dawn the `Device` is a "god object" that contains a lot of facilities useful for the whole object graph that descends from it.
There a number of facilities common to all backends that live in the frontend and backend-specific facilities.
Example of frontend facilities are the management of content-less object caches, or the toggle management.
Example of backend facilities are GPU memory allocators or the backing API function pointer table.

## Frontend facilities

Like in the WebGPU API, `DeviceBase` is the factory for most other objects and contains logic to do handle validation and backend errors.

### Immutable object caches

A number of WebGPU objects are immutable once created, and can be expensive to create, like pipelines.
`DeviceBase` contains caches for these objects so that they are free to create the second time.
This is also useful to be able to compare objects by pointers like `BindGroupLayouts` since two BGLs would be equal iff they are the same object.

### Format Tables

### Error Handling

### Device Lifecycle

### Toggles

Toggles are booleans that control code paths inside of Dawn, like lazy-clearing resources or using D3D12 render passes.
They aren't just booleans close to the code path they control, because embedders of Dawn like Chromium want to be able to surface what toggles are used by a device (like in about:gpu).

Toogles are to be used for any optional code path in Dawn, including:

 - Workarounds for driver bugs.
 - Disabling select parts of the validation or robustness.
 - Enabling limitations that help with testing.
 - Using more advanced or optional API features.

Toggles are defined in a table in [Toggles.cpp](../src/dawn_native/Toggles.cpp) that also includes their name and description.
The name can be used to force enabling of a toggle or, at the contrary, force the disabling of a toogle.
This is particularly useful in tests so that the two sides of a code path can be tested (for example using D3D12 render passes and not).

```
DAWN_INSTANTIATE_TEST(RenderPassTest,
                      D3D12Backend(),
                      D3D12Backend({}, {"use_d3d12_render_pass"}));
// The {} is the list of force enabled toggles, {"..."} the force disabled ones.
```

The initialization order looks as follows:

 - The toggles overrides from the device descriptor are applied.
 - The frontend device default toggles are applied (unless already overriden).
 - The backend device default toggles are applied (unless already overriden) using `DeviceBase::SetToggle`
 - The backend device can ignore overriden toggles if it can't support them by using `DeviceBase::ForceSetToggle`

