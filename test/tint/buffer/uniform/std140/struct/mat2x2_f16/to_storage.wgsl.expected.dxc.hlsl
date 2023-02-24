struct S {
  int before;
  matrix<float16_t, 2, 2> m;
  int after;
};

cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[32];
};
RWByteAddressBuffer s : register(u1, space0);

void tint_symbol_3(uint offset, matrix<float16_t, 2, 2> value) {
  s.Store<vector<float16_t, 2> >((offset + 0u), value[0u]);
  s.Store<vector<float16_t, 2> >((offset + 4u), value[1u]);
}

void tint_symbol_1(uint offset, S value) {
  s.Store((offset + 0u), asuint(value.before));
  tint_symbol_3((offset + 4u), value.m);
  s.Store((offset + 64u), asuint(value.after));
}

void tint_symbol(uint offset, S value[4]) {
  S array_1[4] = value;
  {
    for(uint i = 0u; (i < 4u); i = (i + 1u)) {
      tint_symbol_1((offset + (i * 128u)), array_1[i]);
    }
  }
}

matrix<float16_t, 2, 2> tint_symbol_8(uint offset) {
  const uint scalar_offset = ((offset + 0u)) / 4;
  uint ubo_load = u[scalar_offset / 4][scalar_offset % 4];
  const uint scalar_offset_1 = ((offset + 4u)) / 4;
  uint ubo_load_1 = u[scalar_offset_1 / 4][scalar_offset_1 % 4];
  return matrix<float16_t, 2, 2>(vector<float16_t, 2>(float16_t(f16tof32(ubo_load & 0xFFFF)), float16_t(f16tof32(ubo_load >> 16))), vector<float16_t, 2>(float16_t(f16tof32(ubo_load_1 & 0xFFFF)), float16_t(f16tof32(ubo_load_1 >> 16))));
}

S tint_symbol_6(uint offset) {
  const uint scalar_offset_2 = ((offset + 0u)) / 4;
  const uint scalar_offset_3 = ((offset + 64u)) / 4;
  const S tint_symbol_10 = {asint(u[scalar_offset_2 / 4][scalar_offset_2 % 4]), tint_symbol_8((offset + 4u)), asint(u[scalar_offset_3 / 4][scalar_offset_3 % 4])};
  return tint_symbol_10;
}

typedef S tint_symbol_5_ret[4];
tint_symbol_5_ret tint_symbol_5(uint offset) {
  S arr[4] = (S[4])0;
  {
    for(uint i_1 = 0u; (i_1 < 4u); i_1 = (i_1 + 1u)) {
      arr[i_1] = tint_symbol_6((offset + (i_1 * 128u)));
    }
  }
  return arr;
}

[numthreads(1, 1, 1)]
void f() {
  tint_symbol(0u, tint_symbol_5(0u));
  tint_symbol_1(128u, tint_symbol_6(256u));
  tint_symbol_3(388u, tint_symbol_8(260u));
  uint ubo_load_2 = u[0].z;
  s.Store<vector<float16_t, 2> >(132u, vector<float16_t, 2>(float16_t(f16tof32(ubo_load_2 & 0xFFFF)), float16_t(f16tof32(ubo_load_2 >> 16))).yx);
  return;
}
