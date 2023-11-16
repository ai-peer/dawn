# Memory Heap Info (Experimental!)

The `memory-heap-info` feature allows querying memory heap information from the device.

`size_t wgpu::Device::QueryMemoryHeapInfo(wgpu::MemoryHeapInfo* info)` queries memory information from the device.
It returns the number of memory heaps that exist on the device.
When `info` is not nullptr, it will write out an array of memory heap information to the `info` argument, one per heap.

If `wgpu::FeatureName::MemoryHeapInfo` is not enabled, querying the heap information will generate a validation error.
It will always return zero.

Example Usage:
```
size_t count;
count = device.QueryMemoryHeapInfo(nullptr);

std::vector<wgpu::MemoryHeapInfo> info(count);
device.QueryMemoryHeapInfo(info.data());

for (const wgpu::MemoryHeapInfo& heap : info) {
  printf("%llu\n, heap.maxRecommendedSize);
}
```

Adds `HeapProperty` which is a bitmask describing the type of memory a heap is.
```
enum class HeapProperty : uint32_t {
    Undefined = 0x00000000,
    DeviceLocal = 0x00000001,
    HostVisible = 0x00000002,
    HostCoherent = 0x00000003,
    HostCached = 0x00000004,
};
```

Adds `MemoryHeapInfo` which is a struct describing a memory heap.
```
struct MemoryHeapInfo {
    ChainedStructOut  * nextInChain = nullptr;
    HeapProperty heapProperties;
    uint64_t recommendedMaxSize;
};
```

`MemoryHeapInfo::recommendedMaxSize` is the max size that should be allocated out of this heap. Allocating more than this may result in poor performance or may deterministically run out of memory.
