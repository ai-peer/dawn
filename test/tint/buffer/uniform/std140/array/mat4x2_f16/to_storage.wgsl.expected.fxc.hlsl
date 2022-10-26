SKIP: FAILED

cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[4];
};
RWByteAddressBuffer s : register(u1, space0);

void tint_symbol_1(RWByteAddressBuffer buffer, uint offset, matrix<float16_t, 4, 2> value) {
  buffer.Store<vector<float16_t, 2> >((offset + 0u), value[0u]);
  buffer.Store<vector<float16_t, 2> >((offset + 4u), value[1u]);
  buffer.Store<vector<float16_t, 2> >((offset + 8u), value[2u]);
  buffer.Store<vector<float16_t, 2> >((offset + 12u), value[3u]);
}

void tint_symbol(RWByteAddressBuffer buffer, uint offset, matrix<float16_t, 4, 2> value[4]) {
  matrix<float16_t, 4, 2> array[4] = value;
  {
    for(uint i = 0u; (i < 4u); i = (i + 1u)) {
      tint_symbol_1(buffer, (offset + (i * 16u)), array[i]);
    }
  }
}

matrix<float16_t, 4, 2> tint_symbol_4(uint4 buffer[4], uint offset) {
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

typedef matrix<float16_t, 4, 2> tint_symbol_3_ret[4];
tint_symbol_3_ret tint_symbol_3(uint4 buffer[4], uint offset) {
  matrix<float16_t, 4, 2> arr[4] = (matrix<float16_t, 4, 2>[4])0;
  {
    for(uint i_1 = 0u; (i_1 < 4u); i_1 = (i_1 + 1u)) {
      arr[i_1] = tint_symbol_4(buffer, (offset + (i_1 * 16u)));
    }
  }
  return arr;
}

[numthreads(1, 1, 1)]
void f() {
  tint_symbol(s, 0u, tint_symbol_3(u, 0u));
  tint_symbol_1(s, 16u, tint_symbol_4(u, 32u));
  uint ubo_load_4 = u[0].y;
  s.Store<vector<float16_t, 2> >(16u, vector<float16_t, 2>(float16_t(f16tof32(ubo_load_4 & 0xFFFF)), float16_t(f16tof32(ubo_load_4 >> 16))).yx);
  s.Store<float16_t>(16u, float16_t(f16tof32(((u[0].y) & 0xFFFF))));
  return;
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\buffer\Shader@0x000002664DFD1600(6,68-76): error X3000: syntax error: unexpected token 'float16_t'
D:\Projects\RampUp\dawn\test\tint\buffer\Shader@0x000002664DFD1600(7,3-14): error X3018: invalid subscript 'Store'

