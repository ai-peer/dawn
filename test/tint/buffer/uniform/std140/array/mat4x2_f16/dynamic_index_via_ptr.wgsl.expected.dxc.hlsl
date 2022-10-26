cbuffer cbuffer_a : register(b0, space0) {
  uint4 a[4];
};
static int counter = 0;

int i() {
  counter = (counter + 1);
  return counter;
}

matrix<float16_t, 4, 2> tint_symbol_1(uint4 buffer[4], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  uint ubo_load = buffer[scalar_offset_index / 4][scalar_offset_index % 4];
  const uint scalar_offset_bytes_1 = ((offset + 4u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  uint ubo_load_1 = buffer[scalar_offset_index_1 / 4][scalar_offset_index_1 % 4];
  const uint scalar_offset_bytes_2 = ((offset + 8u));
  const uint scalar_offset_index_2 = scalar_offset_bytes_2 / 4;
  uint ubo_load_2 = buffer[scalar_offset_index_2 / 4][scalar_offset_index_2 % 4];
  const uint scalar_offset_bytes_3 = ((offset + 12u));
  const uint scalar_offset_index_3 = scalar_offset_bytes_3 / 4;
  uint ubo_load_3 = buffer[scalar_offset_index_3 / 4][scalar_offset_index_3 % 4];
  return matrix<float16_t, 4, 2>(vector<float16_t, 2>(float16_t(f16tof32(ubo_load & 0xFFFF)), float16_t(f16tof32(ubo_load >> 16))), vector<float16_t, 2>(float16_t(f16tof32(ubo_load_1 & 0xFFFF)), float16_t(f16tof32(ubo_load_1 >> 16))), vector<float16_t, 2>(float16_t(f16tof32(ubo_load_2 & 0xFFFF)), float16_t(f16tof32(ubo_load_2 >> 16))), vector<float16_t, 2>(float16_t(f16tof32(ubo_load_3 & 0xFFFF)), float16_t(f16tof32(ubo_load_3 >> 16))));
}

typedef matrix<float16_t, 4, 2> tint_symbol_ret[4];
tint_symbol_ret tint_symbol(uint4 buffer[4], uint offset) {
  matrix<float16_t, 4, 2> arr[4] = (matrix<float16_t, 4, 2>[4])0;
  {
    for(uint i_1 = 0u; (i_1 < 4u); i_1 = (i_1 + 1u)) {
      arr[i_1] = tint_symbol_1(buffer, (offset + (i_1 * 16u)));
    }
  }
  return arr;
}

[numthreads(1, 1, 1)]
void f() {
  const int p_a_i_save = i();
  const int p_a_i_i_save = i();
  const matrix<float16_t, 4, 2> l_a[4] = tint_symbol(a, 0u);
  const matrix<float16_t, 4, 2> l_a_i = tint_symbol_1(a, (16u * uint(p_a_i_save)));
  const uint scalar_offset_bytes_4 = (((16u * uint(p_a_i_save)) + (4u * uint(p_a_i_i_save))));
  const uint scalar_offset_index_4 = scalar_offset_bytes_4 / 4;
  uint ubo_load_4 = a[scalar_offset_index_4 / 4][scalar_offset_index_4 % 4];
  const vector<float16_t, 2> l_a_i_i = vector<float16_t, 2>(float16_t(f16tof32(ubo_load_4 & 0xFFFF)), float16_t(f16tof32(ubo_load_4 >> 16)));
  return;
}
