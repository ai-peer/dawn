SKIP: FAILED

struct S {
  int before;
  matrix<float16_t, 4, 2> m;
  int after;
};

cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[32];
};

void a(S a_1[4]) {
}

void b(S s) {
}

void c(matrix<float16_t, 4, 2> m) {
}

void d(vector<float16_t, 2> v) {
}

void e(float16_t f_1) {
}

matrix<float16_t, 4, 2> tint_symbol_3(uint4 buffer[32], uint offset) {
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

S tint_symbol_1(uint4 buffer[32], uint offset) {
  const uint scalar_offset_bytes_4 = ((offset + 0u));
  const uint scalar_offset_index_4 = scalar_offset_bytes_4 / 4;
  const uint scalar_offset_bytes_5 = ((offset + 64u));
  const uint scalar_offset_index_5 = scalar_offset_bytes_5 / 4;
  const S tint_symbol_5 = {asint(buffer[scalar_offset_index_4 / 4][scalar_offset_index_4 % 4]), tint_symbol_3(buffer, (offset + 4u)), asint(buffer[scalar_offset_index_5 / 4][scalar_offset_index_5 % 4])};
  return tint_symbol_5;
}

typedef S tint_symbol_ret[4];
tint_symbol_ret tint_symbol(uint4 buffer[32], uint offset) {
  S arr[4] = (S[4])0;
  {
    for(uint i = 0u; (i < 4u); i = (i + 1u)) {
      arr[i] = tint_symbol_1(buffer, (offset + (i * 128u)));
    }
  }
  return arr;
}

[numthreads(1, 1, 1)]
void f() {
  a(tint_symbol(u, 0u));
  b(tint_symbol_1(u, 256u));
  c(tint_symbol_3(u, 260u));
  uint ubo_load_4 = u[0].z;
  d(vector<float16_t, 2>(float16_t(f16tof32(ubo_load_4 & 0xFFFF)), float16_t(f16tof32(ubo_load_4 >> 16))).yx);
  uint ubo_load_5 = u[0].z;
  e(vector<float16_t, 2>(float16_t(f16tof32(ubo_load_5 & 0xFFFF)), float16_t(f16tof32(ubo_load_5 >> 16))).yx.x);
  return;
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\buffer\Shader@0x0000016B118A2260(3,10-18): error X3000: syntax error: unexpected token 'float16_t'

