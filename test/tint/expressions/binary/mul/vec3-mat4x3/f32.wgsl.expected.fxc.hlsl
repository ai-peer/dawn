cbuffer cbuffer_data : register(b0, space0) {
  uint4 data[5];
};

float4x3 tint_symbol_3(uint4 buffer[5], uint offset) {
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

void main() {
  const float4 x = mul(tint_symbol_3(data, 0u), asfloat(data[4].xyz));
  return;
}
