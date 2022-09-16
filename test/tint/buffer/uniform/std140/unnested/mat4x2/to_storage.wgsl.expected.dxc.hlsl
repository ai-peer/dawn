cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[2];
};
RWByteAddressBuffer s : register(u1, space0);

void tint_symbol(RWByteAddressBuffer buffer, uint offset, float4x2 value) {
  buffer.Store2((offset + 0u), asuint(value[0u]));
  buffer.Store2((offset + 8u), asuint(value[1u]));
  buffer.Store2((offset + 16u), asuint(value[2u]));
  buffer.Store2((offset + 24u), asuint(value[3u]));
}

float4x2 tint_symbol_2(uint4 buffer[2], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  uint4 ubo_load = buffer[scalar_offset_index / 4];
  const uint scalar_offset_bytes_1 = ((offset + 8u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  uint4 ubo_load_1 = buffer[scalar_offset_index_1 / 4];
  const uint scalar_offset_bytes_2 = ((offset + 16u));
  const uint scalar_offset_index_2 = scalar_offset_bytes_2 / 4;
  uint4 ubo_load_2 = buffer[scalar_offset_index_2 / 4];
  const uint scalar_offset_bytes_3 = ((offset + 24u));
  const uint scalar_offset_index_3 = scalar_offset_bytes_3 / 4;
  uint4 ubo_load_3 = buffer[scalar_offset_index_3 / 4];
  return float4x2(asfloat(((scalar_offset_index & 2) ? ubo_load.zw : ubo_load.xy)), asfloat(((scalar_offset_index_1 & 2) ? ubo_load_1.zw : ubo_load_1.xy)), asfloat(((scalar_offset_index_2 & 2) ? ubo_load_2.zw : ubo_load_2.xy)), asfloat(((scalar_offset_index_3 & 2) ? ubo_load_3.zw : ubo_load_3.xy)));
}

[numthreads(1, 1, 1)]
void f() {
  tint_symbol(s, 0u, tint_symbol_2(u, 0u));
  s.Store2(8u, asuint(asfloat(u[0].xy)));
  s.Store2(8u, asuint(asfloat(u[0].xy).yx));
  s.Store(4u, asuint(asfloat(u[0].z)));
  return;
}
