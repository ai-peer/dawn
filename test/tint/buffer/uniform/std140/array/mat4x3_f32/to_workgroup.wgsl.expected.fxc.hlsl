cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[16];
};
groupshared float4x3 w[4];

struct tint_symbol_1 {
  uint local_invocation_index : SV_GroupIndex;
};

float4x3 tint_symbol_3(uint4 buffer[16], uint offset) {
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

typedef float4x3 tint_symbol_2_ret[4];
tint_symbol_2_ret tint_symbol_2(uint4 buffer[16], uint offset) {
  float4x3 arr[4] = (float4x3[4])0;
  {
    for(uint i_1 = 0u; (i_1 < 4u); i_1 = (i_1 + 1u)) {
      arr[i_1] = tint_symbol_3(buffer, (offset + (i_1 * 64u)));
    }
  }
  return arr;
}

void f_inner(uint local_invocation_index) {
  {
    for(uint idx = local_invocation_index; (idx < 4u); idx = (idx + 1u)) {
      const uint i = idx;
      w[i] = float4x3((0.0f).xxx, (0.0f).xxx, (0.0f).xxx, (0.0f).xxx);
    }
  }
  GroupMemoryBarrierWithGroupSync();
  w = tint_symbol_2(u, 0u);
  w[1] = tint_symbol_3(u, 128u);
  w[1][0] = asfloat(u[1].xyz).zxy;
  w[1][0].x = asfloat(u[1].x);
}

[numthreads(1, 1, 1)]
void f(tint_symbol_1 tint_symbol) {
  f_inner(tint_symbol.local_invocation_index);
  return;
}
