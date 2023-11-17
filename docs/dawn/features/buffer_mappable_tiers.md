# Buffer Mappable Tiers

## wgpu::Feature::BufferMappableTier1:
- TODO(https://crbug.com/dawn/2204): define tier1 feature where index buffer's mapping is restricted due to security or other limitations.

## wgpu::Feature::BufferMappableTier2:
 - The `wgpu::Feature::BufferMappableTier2` feature allows creating a buffer with `wgpu::BufferUsage::MapRead` or `wgpu::BufferUsage::MapWrite` and any other `wgpu::BufferUsage`.
 - Exception is that `wgpu::BufferUsage::MapRead` cannot be combined with `wgpu::BufferUsage::MapWrite` and vice versa.

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

