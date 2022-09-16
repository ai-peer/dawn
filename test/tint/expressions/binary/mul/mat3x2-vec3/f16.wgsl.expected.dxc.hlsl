cbuffer cbuffer_data : register(b0, space0) {
  uint4 data[2];
};

matrix<float16_t, 3, 2> tint_symbol_2(uint4 buffer[2], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  uint ubo_load = buffer[scalar_offset_index / 4][scalar_offset_index % 4];
  const uint scalar_offset_bytes_1 = ((offset + 4u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  uint ubo_load_1 = buffer[scalar_offset_index_1 / 4][scalar_offset_index_1 % 4];
  const uint scalar_offset_bytes_2 = ((offset + 8u));
  const uint scalar_offset_index_2 = scalar_offset_bytes_2 / 4;
  uint ubo_load_2 = buffer[scalar_offset_index_2 / 4][scalar_offset_index_2 % 4];
  return matrix<float16_t, 3, 2>(vector<float16_t, 2>(float16_t(f16tof32(ubo_load & 0xFFFF)), float16_t(f16tof32(ubo_load >> 16))), vector<float16_t, 2>(float16_t(f16tof32(ubo_load_1 & 0xFFFF)), float16_t(f16tof32(ubo_load_1 >> 16))), vector<float16_t, 2>(float16_t(f16tof32(ubo_load_2 & 0xFFFF)), float16_t(f16tof32(ubo_load_2 >> 16))));
}

void main() {
  uint2 ubo_load_3 = data[1].xy;
  vector<float16_t, 2> ubo_load_3_xz = vector<float16_t, 2>(f16tof32(ubo_load_3 & 0xFFFF));
  float16_t ubo_load_3_y = f16tof32(ubo_load_3[0] >> 16);
  const vector<float16_t, 2> x = mul(vector<float16_t, 3>(ubo_load_3_xz[0], ubo_load_3_y, ubo_load_3_xz[1]), tint_symbol_2(data, 0u));
  return;
}
