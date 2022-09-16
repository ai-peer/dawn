cbuffer cbuffer_ubo : register(b0, space0) {
  uint4 ubo[5];
};

RWByteAddressBuffer result : register(u2, space0);

[numthreads(1, 1, 1)]
void f() {
  const uint scalar_offset_bytes = ((16u * uint(asint(ubo[4].x))));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  result.Store(0u, asuint(asint(ubo[scalar_offset_index / 4][scalar_offset_index % 4])));
  return;
}
