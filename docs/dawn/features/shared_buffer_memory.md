# Shared Buffer Memory

## Overview

TODO(dawn:2382): Add feature name(s) when implementation is complete.

```c++
wgpu::SharedBufferMemoryFooBarDescriptor fooBarDesc = {
  .fooBar = ...,
};
wgpu::SharedBufferMemoryDescriptor desc = {};
desc.nextInChain = &fooBarDesc;

wgpu::SharedBufferMemory memory = device.CreateSharedBufferMemory(&desc);
```

After creating the memory, the properties such as the supported `wgpu::BufferUsage` can be queried.
```c++
wgpu::SharedBufferMemoryProperties properties;
memory.GetProperties(&properties);

switch (properties.usage) {
  // ... handle supported usage
}
```

Then, a buffer can be created from it. For example:
```c++
wgpu::BufferDescriptor bufferDesc = {
  // usage must be a subset of the buffer memory's usage
  .usage = properties.usage
};
wgpu::Buffer buffer = memory.CreateBuffer(&bufferDesc);
```

Buffer created from shared buffer memory are not valid to use inside a queue operation until access to the memory is explicitly started using `BeginAccess`. Access is ended using `EndAccess`. For example:

```c++
wgpu::BufferDescriptor bufferDesc = { ... };
wgpu::Buffer buffer = memory.CreateBuffer(&bufferDesc);

// It is valid to create a bind group and encode commands
// referencing the buffer.
wgpu::BindGroup bg = device.CreateBindGroup({..., buffer, ... });
wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
wgpu::ComputePass pass = encoder.BeginComputePass();
// ...
pass.SetBindGroup(0, bg);
// ...
pass.End();
wgpu::CommandBuffer commandBuffer = encoder.Finish();

// Begin/EndAccess must wrap usage of the buffer on the queue.
wgpu::SharedBufferMemoryBeginAccessDescriptor beginAccessDesc = { ... };
memory.BeginAccess(buffer, &beginAccessDesc);

queue.writeBuffer(buffer, ...);
queue.submit(1, &commandBuffer);

wgpu::SharedBufferMemoryEndAccessState endAccessDesc = { ... };
memory.EndAccess(buffer, &endAccessDesc);
```

Multiple buffers may be created from the same SharedBufferMemory, but only one buffer may have access to the memory at a time.

The Begin/End access descriptors are used to pass data necessary to update and synchronize buffer state since the buffer may have been externally used. Chained structs that are supported or required vary depending on the memory type. These access descriptors communicate the buffer's initialized state. They are also used to pass [Shared Fences](./shared_fence.md) to synchronize access to the buffer. When passed in BeginAccess, the buffer will wait for the shared fences to pass before use on the GPU, and on EndAccess, the shared fences for the buffer's last usage will be exported. Passing shared fences to BeginAccess allows Dawn to wait on externally-enqueued GPU work, and exporting shared fences from EndAccess then allows other external code to wait for Dawn's GPU work to complete.