cbuffer cbuffer_m : register(b0, space0) {
  uint4 m[4];
};
static int counter = 0;

int i() {
  counter = (counter + 1);
  return counter;
}

float4x3 tint_symbol(uint4 buffer[4], uint offset) {
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
  const int p_m_i_save = i();
  const float4x3 l_m = tint_symbol(m, 0u);
  const uint scalar_offset_bytes_4 = ((16u * uint(p_m_i_save)));
  const uint scalar_offset_index_4 = scalar_offset_bytes_4 / 4;
  const float3 l_m_i = asfloat(m[scalar_offset_index_4 / 4].xyz);
  return;
}
