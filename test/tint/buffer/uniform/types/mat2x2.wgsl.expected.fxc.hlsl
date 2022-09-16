cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[1];
};

float2x2 tint_symbol(uint4 buffer[1], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  uint4 ubo_load = buffer[scalar_offset_index / 4];
  const uint scalar_offset_bytes_1 = ((offset + 8u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  uint4 ubo_load_1 = buffer[scalar_offset_index_1 / 4];
  return float2x2(asfloat(((scalar_offset_index & 2) ? ubo_load.zw : ubo_load.xy)), asfloat(((scalar_offset_index_1 & 2) ? ubo_load_1.zw : ubo_load_1.xy)));
}

[numthreads(1, 1, 1)]
void main() {
  const float2x2 x = tint_symbol(u, 0u);
  return;
}
