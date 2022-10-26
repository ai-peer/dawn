struct S {
  int before;
  float4x4 m;
  int after;
};

cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[48];
};
static S p[4] = (S[4])0;

float4x4 tint_symbol_3(uint4 buffer[48], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  const uint scalar_offset_bytes_1 = ((offset + 16u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  const uint scalar_offset_bytes_2 = ((offset + 32u));
  const uint scalar_offset_index_2 = scalar_offset_bytes_2 / 4;
  const uint scalar_offset_bytes_3 = ((offset + 48u));
  const uint scalar_offset_index_3 = scalar_offset_bytes_3 / 4;
  return float4x4(asfloat(buffer[scalar_offset_index / 4]), asfloat(buffer[scalar_offset_index_1 / 4]), asfloat(buffer[scalar_offset_index_2 / 4]), asfloat(buffer[scalar_offset_index_3 / 4]));
}

S tint_symbol_1(uint4 buffer[48], uint offset) {
  const uint scalar_offset_bytes_4 = ((offset + 0u));
  const uint scalar_offset_index_4 = scalar_offset_bytes_4 / 4;
  const uint scalar_offset_bytes_5 = ((offset + 128u));
  const uint scalar_offset_index_5 = scalar_offset_bytes_5 / 4;
  const S tint_symbol_5 = {asint(buffer[scalar_offset_index_4 / 4][scalar_offset_index_4 % 4]), tint_symbol_3(buffer, (offset + 16u)), asint(buffer[scalar_offset_index_5 / 4][scalar_offset_index_5 % 4])};
  return tint_symbol_5;
}

typedef S tint_symbol_ret[4];
tint_symbol_ret tint_symbol(uint4 buffer[48], uint offset) {
  S arr[4] = (S[4])0;
  {
    for(uint i = 0u; (i < 4u); i = (i + 1u)) {
      arr[i] = tint_symbol_1(buffer, (offset + (i * 192u)));
    }
  }
  return arr;
}

[numthreads(1, 1, 1)]
void f() {
  p = tint_symbol(u, 0u);
  p[1] = tint_symbol_1(u, 384u);
  p[3].m = tint_symbol_3(u, 400u);
  p[1].m[0] = asfloat(u[2]).ywxz;
  return;
}
