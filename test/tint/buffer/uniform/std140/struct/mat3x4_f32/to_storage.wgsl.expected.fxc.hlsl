struct S {
  int before;
  float3x4 m;
  int after;
};

cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[32];
};
RWByteAddressBuffer s : register(u1, space0);

void tint_symbol_3(uint offset, float3x4 value) {
  s.Store4((offset + 0u), asuint(value[0u]));
  s.Store4((offset + 16u), asuint(value[1u]));
  s.Store4((offset + 32u), asuint(value[2u]));
}

void tint_symbol_1(uint offset, S value) {
  s.Store((offset + 0u), asuint(value.before));
  tint_symbol_3((offset + 16u), value.m);
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

float3x4 tint_symbol_8(uint offset) {
  const uint scalar_offset = ((offset + 0u)) / 4;
  const uint scalar_offset_1 = ((offset + 16u)) / 4;
  const uint scalar_offset_2 = ((offset + 32u)) / 4;
  return float3x4(asfloat(u[scalar_offset / 4]), asfloat(u[scalar_offset_1 / 4]), asfloat(u[scalar_offset_2 / 4]));
}

S tint_symbol_6(uint offset) {
  const uint scalar_offset_3 = ((offset + 0u)) / 4;
  const uint scalar_offset_4 = ((offset + 64u)) / 4;
  const S tint_symbol_10 = {asint(u[scalar_offset_3 / 4][scalar_offset_3 % 4]), tint_symbol_8((offset + 16u)), asint(u[scalar_offset_4 / 4][scalar_offset_4 % 4])};
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
  tint_symbol_3(400u, tint_symbol_8(272u));
  s.Store4(144u, asuint(asfloat(u[2]).ywxz));
  return;
}
