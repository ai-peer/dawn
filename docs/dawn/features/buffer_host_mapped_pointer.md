# Buffer Host Mapped Pointer (Experimental!)

## Overview

Allows creation of buffers from host-mapped pointers. These may be pointers from shared memory allocations, memory-mapped files, etc. The created buffer does not own a reference to the underlying memory so any use of it will become undefined if the host memory is unmapped or freed.

Example usage on Mac:
```c++
// Allocate some mapped / virtual memory. Mac specific allocation bits shown.
vm_address_t addr = 0;
EXPECT_EQ(vm_allocate(mach_task_self(), &addr, size, /* anywhere */ true), KERN_SUCCESS);

void* ptr = reinterpret_cast<void*>(addr);

wgpu::BufferHostMappedPointer hostMappedDesc;
hostMappedDesc.pointer = ptr;

// Use the dispose callback to be notified when the buffer is destroyed and no
// longer in use on the GPU. After this point, it is safe to unmap or deallocate
// the memory.
hostMappedDesc.disposeCallback = [](void* userdata) {
  // Unmap / deallocate the memory. Mac-specific deallocation bits shown.
  auto* data = reinterpret_cast<std::pair<vm_address_t, size_t>*>(userdata);
  vm_deallocate(mach_task_self(), data->first, data->second);
  delete data;
};
hostMappedDesc.userdata = new std::pair<vm_address_t, size_t>(addr, size);

wgpu::BufferDescriptor bufferDesc;
bufferDesc.usage = usage;
bufferDesc.size = size;
bufferDesc.nextInChain = &hostMappedDesc;

wgpu::Buffer buffer = device.CreateBuffer(&bufferDesc);
```

TODO:
 - Figure out interaction with buffer mapping. These types of buffers are essentially persistently mapped.
 - Figure out limits for required pointer and size alignments.
   Probably page-size aligned?
