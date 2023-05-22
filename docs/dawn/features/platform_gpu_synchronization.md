# Platform GPU Synchronization
Synchronization between the platform and the GPU is provided by various platform-specific of optional features.

## Vulkan
Dawn supports importing and exporting external sempahore handles. This can only be done when wrapping an external texture. Below shows a table of features, the enum for the sempahore, and the associated Vulkan semaphore type it may be used with.

| Feature | Semaphore Type | Vulkan External Semaphore Handle Type |
| ------- | --------- | --------------------------------------|
| sync-vk-semaphore-opaque-fd | wgpu::DawnVkSemaphoreTypeOpaqueFD | VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT |
| sync-vk-semaphore-sync-fd | wgpu::DawnVkSemaphoreTypeSyncFD | VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT |
| sync-vk-semaphore-zircon-handle | wgpu::DawnVkSemaphoreTypeZirconHandle |VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA |

### Usage
The semaphore type, such as `wgpu::DawnVkSemaphoreTypeSyncFD` must be passed in the descriptor of `ExternalImageDescriptorVk`. This type describes the type of semaphore handles that `ExternalImageDescriptorVk::waitFDs` refers to, and the type of semaphore handle that should be exported in `ExportVulkanImage` when access to the wrapped `wgpu::Texture` ends.
