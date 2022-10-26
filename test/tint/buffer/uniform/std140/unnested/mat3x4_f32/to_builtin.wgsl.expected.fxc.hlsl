cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[3];
};

float3x4 tint_symbol(uint4 buffer[3], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  const uint scalar_offset_bytes_1 = ((offset + 16u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  const uint scalar_offset_bytes_2 = ((offset + 32u));
  const uint scalar_offset_index_2 = scalar_offset_bytes_2 / 4;
  return float3x4(asfloat(buffer[scalar_offset_index / 4]), asfloat(buffer[scalar_offset_index_1 / 4]), asfloat(buffer[scalar_offset_index_2 / 4]));
}

[numthreads(1, 1, 1)]
void f() {
  const float4x3 t = transpose(tint_symbol(u, 0u));
  const float l = length(asfloat(u[1]));
  const float a = abs(asfloat(u[0]).ywxz.x);
  return;
}
