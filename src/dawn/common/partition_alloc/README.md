This directory provide Dawn a "no-op" implementation of partition_alloc's
raw_ptr files. This is used only when `dawn_partition_alloc_dir` is not set.

# Why not using the PartitionAlloc "no-op" implementation?
For now, we decided Dawn to provide its own. This allow the dependency to be
optional and easier to integrate into other build system like CMake or Bazel.

Moreover Dawn and PartitionAlloc depends on Chrome's //build. Skia do not have
this dependency.
PartitionAlloc is going to remove this dependency for being used inside Skia.
Until it happened, it means Skia is not going to set `dawn_partition_alloc_dir`
and will rely on this implementation.

# TODO(arthursonzogni): https://crbug.com/1464560)

Provide additional files:
- partition_alloc/pointers/raw_ref.h
- partition_alloc/pointers/raw_ptr_exclusion.h
- partition_alloc/pointers/raw_ptr_cast.h

Expand the raw_ptr implementation:
- RawPtrTraits
- raw_ptr::* extra functions.
- etc...
