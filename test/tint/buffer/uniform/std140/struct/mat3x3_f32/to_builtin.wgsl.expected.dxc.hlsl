cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[32];
};

float3x3 tint_symbol(uint4 buffer[32], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  const uint scalar_offset_bytes_1 = ((offset + 16u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  const uint scalar_offset_bytes_2 = ((offset + 32u));
  const uint scalar_offset_index_2 = scalar_offset_bytes_2 / 4;
  return float3x3(asfloat(buffer[scalar_offset_index / 4].xyz), asfloat(buffer[scalar_offset_index_1 / 4].xyz), asfloat(buffer[scalar_offset_index_2 / 4].xyz));
}

[numthreads(1, 1, 1)]
void f() {
  const float3x3 t = transpose(tint_symbol(u, 272u));
  const float l = length(asfloat(u[2].xyz).zxy);
  const float a = abs(asfloat(u[2].xyz).zxy.x);
  return;
}
