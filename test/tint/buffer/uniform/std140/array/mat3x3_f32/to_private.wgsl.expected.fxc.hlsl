cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[12];
};
static float3x3 p[4] = (float3x3[4])0;

float3x3 tint_symbol_1(uint4 buffer[12], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  const uint scalar_offset_bytes_1 = ((offset + 16u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  const uint scalar_offset_bytes_2 = ((offset + 32u));
  const uint scalar_offset_index_2 = scalar_offset_bytes_2 / 4;
  return float3x3(asfloat(buffer[scalar_offset_index / 4].xyz), asfloat(buffer[scalar_offset_index_1 / 4].xyz), asfloat(buffer[scalar_offset_index_2 / 4].xyz));
}

typedef float3x3 tint_symbol_ret[4];
tint_symbol_ret tint_symbol(uint4 buffer[12], uint offset) {
  float3x3 arr[4] = (float3x3[4])0;
  {
    for(uint i = 0u; (i < 4u); i = (i + 1u)) {
      arr[i] = tint_symbol_1(buffer, (offset + (i * 48u)));
    }
  }
  return arr;
}

[numthreads(1, 1, 1)]
void f() {
  p = tint_symbol(u, 0u);
  p[1] = tint_symbol_1(u, 96u);
  p[1][0] = asfloat(u[1].xyz).zxy;
  p[1][0].x = asfloat(u[1].x);
  return;
}
