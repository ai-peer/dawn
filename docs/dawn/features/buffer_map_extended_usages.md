# Buffer Map Extended Usages

## Overview:
 - The `wgpu::Feature::BufferMapExtendedUsages` feature allows creating a buffer with `wgpu::BufferUsage::MapRead` and/or `wgpu::BufferUsage::MapWrite` and any other `wgpu::BufferUsage`.
 - The `wgpu::Feature::BufferMapWriteExtendedUsages` feature allows:
   - Creating a buffer with `wgpu::BufferUsage::MapWrite` and any other `wgpu::BufferUsage`, except:
     - `wgpu::BufferUsage::MapRead`.
     - `wgpu::BufferUsage::CopyDst`.
     - `wgpu::BufferUsage::Indirect`.
     - `wgpu::BufferUsage::QueryResolve`.
   - If `wgpu::BufferUsage::MapWrite` is combined with `wgpu::BufferUsage::Uniform`, the only other usage allowed is `wgpu::CopySrc`.
   - If a buffer is created with `wgpu::BufferUsage::MapWrite` and `wgpu::BufferUsage::Storage`, it can only be bound to a bind group with `wgpu::BufferBindingType::ReadOnlyStorage`.

### Example Usage:
```
wgpu::BufferDescriptor descriptor;
descriptor.size = size;
descriptor.usage = wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::Uniform;
wgpu::Buffer uniformBuffer = device.CreateBuffer(&descriptor);

uniformBuffer.MapAsync(wgpu::MapMode::Write, 0, size,
   [](WGPUBufferMapAsyncStatus status, void* userdata)
   {
      wgpu::Buffer* buffer = static_cast<wgpu::Buffer*>(userdata);
      memcpy(buffer->GetMappedRange(), data, sizeof(data));
   },
   &uniformBuffer);
```

