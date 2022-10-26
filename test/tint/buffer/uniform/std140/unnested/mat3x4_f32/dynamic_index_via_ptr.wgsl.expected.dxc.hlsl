cbuffer cbuffer_m : register(b0, space0) {
  uint4 m[3];
};
static int counter = 0;

int i() {
  counter = (counter + 1);
  return counter;
}

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
  const int p_m_i_save = i();
  const float3x4 l_m = tint_symbol(m, 0u);
  const uint scalar_offset_bytes_3 = ((16u * uint(p_m_i_save)));
  const uint scalar_offset_index_3 = scalar_offset_bytes_3 / 4;
  const float4 l_m_i = asfloat(m[scalar_offset_index_3 / 4]);
  return;
}
