cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[3];
};
groupshared float3x3 w;

struct tint_symbol_1 {
  uint local_invocation_index : SV_GroupIndex;
};

float3x3 tint_symbol_2(uint4 buffer[3], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  const uint scalar_offset_bytes_1 = ((offset + 16u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  const uint scalar_offset_bytes_2 = ((offset + 32u));
  const uint scalar_offset_index_2 = scalar_offset_bytes_2 / 4;
  return float3x3(asfloat(buffer[scalar_offset_index / 4].xyz), asfloat(buffer[scalar_offset_index_1 / 4].xyz), asfloat(buffer[scalar_offset_index_2 / 4].xyz));
}

void f_inner(uint local_invocation_index) {
  {
    w = float3x3((0.0f).xxx, (0.0f).xxx, (0.0f).xxx);
  }
  GroupMemoryBarrierWithGroupSync();
  w = tint_symbol_2(u, 0u);
  w[1] = asfloat(u[0].xyz);
  w[1] = asfloat(u[0].xyz).zxy;
  w[0][1] = asfloat(u[1].x);
}

[numthreads(1, 1, 1)]
void f(tint_symbol_1 tint_symbol) {
  f_inner(tint_symbol.local_invocation_index);
  return;
}
