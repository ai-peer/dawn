# Buffer Host Mapped Pointer (Experimental!)

## Overview

Enable this feature by requesting `wgpu::FeatureName::BufferHostMappedPointer` on device creation.

Allows creation of buffers from host-mapped pointers. These may be pointers from shared memory allocations, memory-mapped files, etc. The created buffer does not own a reference to the underlying memory so any use of it will become undefined if the host memory is unmapped or freed.

Example usage:
```c++
// Memory map a file.
void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

wgpu::BufferHostMappedPointer hostMappedDesc;
hostMappedDesc.pointer = ptr;

// Use the dispose callback to be notified when the buffer is destroyed and no
// longer in use on the GPU. After this point, it is safe to unmap the memory.
hostMappedDesc.disposeCallback = [](void* userdata) {
  // Unmap and cose the file.
  auto* data = reinterpret_cast<std::tuple<int, void*, size_t>*>(userdata);
  munmap(std::get<1>(*data), std::get<2>(*data));
  close(std::get<0>(*data));
  delete data;
};
// Pass the fd, ptr, and size as the userdata so we can free the
// memory later.
hostMappedDesc.userdata = new std::tuple<int, void*, size_t>(fd, ptr, size);

wgpu::BufferDescriptor bufferDesc;
bufferDesc.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::MapWrite;
bufferDesc.size = size;
bufferDesc.nextInChain = &hostMappedDesc;

wgpu::Buffer buffer = device.CreateBuffer(&bufferDesc);
```

TODO:
 - Figure out interaction with buffer mapping. These types of buffers are essentially persistently mapped.
