# Shared Fence (Experimental!)

## Overview

A variety of features may be used to externally synchronize usage of resources on the GPU.

- `wgpu::FeatureName::SharedFenceVkSemaphoreOpaqueFD`
- `wgpu::FeatureName::SharedFenceVkSemaphoreSyncFD`
- `wgpu::FeatureName::SharedFenceVkSemaphoreZirconHandle`
- `wgpu::FeatureName::SharedFenceDXGISharedHandle`
- `wgpu::FeatureName::SharedFenceMTLSharedEvent`

TODO(crbug.com/dawn/1745): additional documentation
