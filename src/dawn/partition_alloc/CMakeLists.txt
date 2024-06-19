dawn_add_library(
  partition_alloc
  HEADER_ONLY
  UTILITY_TARGET dawn::dawnbuild
  HEADERS
    "partition_alloc/pointers/raw_ptr_exclusion.h"
    "partition_alloc/pointers/raw_ptr.h"
    "partition_alloc/pointers/raw_ref.h"
)

target_include_directories(partition_alloc INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
