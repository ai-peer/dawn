cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[16];
};
RWByteAddressBuffer s : register(u1, space0);

void tint_symbol_1(uint offset, float4x4 value) {
  s.Store4((offset + 0u), asuint(value[0u]));
  s.Store4((offset + 16u), asuint(value[1u]));
  s.Store4((offset + 32u), asuint(value[2u]));
  s.Store4((offset + 48u), asuint(value[3u]));
}

void tint_symbol(uint offset, float4x4 value[4]) {
  float4x4 array_1[4] = value;
  {
    for(uint i = 0u; (i < 4u); i = (i + 1u)) {
      tint_symbol_1((offset + (i * 64u)), array_1[i]);
    }
  }
}

float4x4 tint_symbol_4(uint offset) {
  const uint scalar_offset = ((offset + 0u)) / 4;
  const uint scalar_offset_1 = ((offset + 16u)) / 4;
  const uint scalar_offset_2 = ((offset + 32u)) / 4;
  const uint scalar_offset_3 = ((offset + 48u)) / 4;
  return float4x4(asfloat(u[scalar_offset / 4]), asfloat(u[scalar_offset_1 / 4]), asfloat(u[scalar_offset_2 / 4]), asfloat(u[scalar_offset_3 / 4]));
}

typedef float4x4 tint_symbol_3_ret[4];
tint_symbol_3_ret tint_symbol_3(uint offset) {
  float4x4 arr[4] = (float4x4[4])0;
  {
    for(uint i_1 = 0u; (i_1 < 4u); i_1 = (i_1 + 1u)) {
      arr[i_1] = tint_symbol_4((offset + (i_1 * 64u)));
    }
  }
  return arr;
}

[numthreads(1, 1, 1)]
void f() {
  tint_symbol(0u, tint_symbol_3(0u));
  tint_symbol_1(64u, tint_symbol_4(128u));
  s.Store4(64u, asuint(asfloat(u[1]).ywxz));
  s.Store(64u, asuint(asfloat(u[1].x)));
  return;
}
