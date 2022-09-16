cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[2];
};

float2x3 tint_symbol(uint4 buffer[2], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  const uint scalar_offset_bytes_1 = ((offset + 16u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  return float2x3(asfloat(buffer[scalar_offset_index / 4].xyz), asfloat(buffer[scalar_offset_index_1 / 4].xyz));
}

[numthreads(1, 1, 1)]
void main() {
  const float2x3 x = tint_symbol(u, 0u);
  return;
}
