struct S {
  int before;
  float3x4 m;
  int after;
};

cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[32];
};
groupshared S w[4];

struct tint_symbol_1 {
  uint local_invocation_index : SV_GroupIndex;
};

float3x4 tint_symbol_5(uint4 buffer[32], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  const uint scalar_offset_bytes_1 = ((offset + 16u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  const uint scalar_offset_bytes_2 = ((offset + 32u));
  const uint scalar_offset_index_2 = scalar_offset_bytes_2 / 4;
  return float3x4(asfloat(buffer[scalar_offset_index / 4]), asfloat(buffer[scalar_offset_index_1 / 4]), asfloat(buffer[scalar_offset_index_2 / 4]));
}

S tint_symbol_3(uint4 buffer[32], uint offset) {
  const uint scalar_offset_bytes_3 = ((offset + 0u));
  const uint scalar_offset_index_3 = scalar_offset_bytes_3 / 4;
  const uint scalar_offset_bytes_4 = ((offset + 64u));
  const uint scalar_offset_index_4 = scalar_offset_bytes_4 / 4;
  const S tint_symbol_8 = {asint(buffer[scalar_offset_index_3 / 4][scalar_offset_index_3 % 4]), tint_symbol_5(buffer, (offset + 16u)), asint(buffer[scalar_offset_index_4 / 4][scalar_offset_index_4 % 4])};
  return tint_symbol_8;
}

typedef S tint_symbol_2_ret[4];
tint_symbol_2_ret tint_symbol_2(uint4 buffer[32], uint offset) {
  S arr[4] = (S[4])0;
  {
    for(uint i_1 = 0u; (i_1 < 4u); i_1 = (i_1 + 1u)) {
      arr[i_1] = tint_symbol_3(buffer, (offset + (i_1 * 128u)));
    }
  }
  return arr;
}

void f_inner(uint local_invocation_index) {
  {
    for(uint idx = local_invocation_index; (idx < 4u); idx = (idx + 1u)) {
      const uint i = idx;
      const S tint_symbol_7 = (S)0;
      w[i] = tint_symbol_7;
    }
  }
  GroupMemoryBarrierWithGroupSync();
  w = tint_symbol_2(u, 0u);
  w[1] = tint_symbol_3(u, 256u);
  w[3].m = tint_symbol_5(u, 272u);
  w[1].m[0] = asfloat(u[2]).ywxz;
}

[numthreads(1, 1, 1)]
void f(tint_symbol_1 tint_symbol) {
  f_inner(tint_symbol.local_invocation_index);
  return;
}
