cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[4];
};
RWByteAddressBuffer s : register(u1, space0);

void tint_symbol(RWByteAddressBuffer buffer, uint offset, float4x3 value) {
  buffer.Store3((offset + 0u), asuint(value[0u]));
  buffer.Store3((offset + 16u), asuint(value[1u]));
  buffer.Store3((offset + 32u), asuint(value[2u]));
  buffer.Store3((offset + 48u), asuint(value[3u]));
}

float4x3 tint_symbol_2(uint4 buffer[4], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  const uint scalar_offset_bytes_1 = ((offset + 16u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  const uint scalar_offset_bytes_2 = ((offset + 32u));
  const uint scalar_offset_index_2 = scalar_offset_bytes_2 / 4;
  const uint scalar_offset_bytes_3 = ((offset + 48u));
  const uint scalar_offset_index_3 = scalar_offset_bytes_3 / 4;
  return float4x3(asfloat(buffer[scalar_offset_index / 4].xyz), asfloat(buffer[scalar_offset_index_1 / 4].xyz), asfloat(buffer[scalar_offset_index_2 / 4].xyz), asfloat(buffer[scalar_offset_index_3 / 4].xyz));
}

[numthreads(1, 1, 1)]
void f() {
  tint_symbol(s, 0u, tint_symbol_2(u, 0u));
  s.Store3(16u, asuint(asfloat(u[0].xyz)));
  s.Store3(16u, asuint(asfloat(u[0].xyz).zxy));
  s.Store(4u, asuint(asfloat(u[1].x)));
  return;
}
