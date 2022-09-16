cbuffer cbuffer_s : register(b0, space0) {
  uint4 s[96];
};

struct tint_symbol_1 {
  uint idx : SV_GroupIndex;
};

float2x3 tint_symbol_9(uint4 buffer[96], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  const uint scalar_offset_bytes_1 = ((offset + 16u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  return float2x3(asfloat(buffer[scalar_offset_index / 4].xyz), asfloat(buffer[scalar_offset_index_1 / 4].xyz));
}

float3x2 tint_symbol_10(uint4 buffer[96], uint offset) {
  const uint scalar_offset_bytes_2 = ((offset + 0u));
  const uint scalar_offset_index_2 = scalar_offset_bytes_2 / 4;
  uint4 ubo_load = buffer[scalar_offset_index_2 / 4];
  const uint scalar_offset_bytes_3 = ((offset + 8u));
  const uint scalar_offset_index_3 = scalar_offset_bytes_3 / 4;
  uint4 ubo_load_1 = buffer[scalar_offset_index_3 / 4];
  const uint scalar_offset_bytes_4 = ((offset + 16u));
  const uint scalar_offset_index_4 = scalar_offset_bytes_4 / 4;
  uint4 ubo_load_2 = buffer[scalar_offset_index_4 / 4];
  return float3x2(asfloat(((scalar_offset_index_2 & 2) ? ubo_load.zw : ubo_load.xy)), asfloat(((scalar_offset_index_3 & 2) ? ubo_load_1.zw : ubo_load_1.xy)), asfloat(((scalar_offset_index_4 & 2) ? ubo_load_2.zw : ubo_load_2.xy)));
}

typedef int4 tint_symbol_12_ret[4];
tint_symbol_12_ret tint_symbol_12(uint4 buffer[96], uint offset) {
  int4 arr_1[4] = (int4[4])0;
  {
    for(uint i_1 = 0u; (i_1 < 4u); i_1 = (i_1 + 1u)) {
      const uint scalar_offset_bytes_5 = ((offset + (i_1 * 16u)));
      const uint scalar_offset_index_5 = scalar_offset_bytes_5 / 4;
      arr_1[i_1] = asint(buffer[scalar_offset_index_5 / 4]);
    }
  }
  return arr_1;
}

void main_inner(uint idx) {
  const uint scalar_offset_bytes_6 = ((192u * idx));
  const uint scalar_offset_index_6 = scalar_offset_bytes_6 / 4;
  const int3 a = asint(s[scalar_offset_index_6 / 4].xyz);
  const uint scalar_offset_bytes_7 = (((192u * idx) + 12u));
  const uint scalar_offset_index_7 = scalar_offset_bytes_7 / 4;
  const int b = asint(s[scalar_offset_index_7 / 4][scalar_offset_index_7 % 4]);
  const uint scalar_offset_bytes_8 = (((192u * idx) + 16u));
  const uint scalar_offset_index_8 = scalar_offset_bytes_8 / 4;
  const uint3 c = s[scalar_offset_index_8 / 4].xyz;
  const uint scalar_offset_bytes_9 = (((192u * idx) + 28u));
  const uint scalar_offset_index_9 = scalar_offset_bytes_9 / 4;
  const uint d = s[scalar_offset_index_9 / 4][scalar_offset_index_9 % 4];
  const uint scalar_offset_bytes_10 = (((192u * idx) + 32u));
  const uint scalar_offset_index_10 = scalar_offset_bytes_10 / 4;
  const float3 e = asfloat(s[scalar_offset_index_10 / 4].xyz);
  const uint scalar_offset_bytes_11 = (((192u * idx) + 44u));
  const uint scalar_offset_index_11 = scalar_offset_bytes_11 / 4;
  const float f = asfloat(s[scalar_offset_index_11 / 4][scalar_offset_index_11 % 4]);
  const uint scalar_offset_bytes_12 = (((192u * idx) + 48u));
  const uint scalar_offset_index_12 = scalar_offset_bytes_12 / 4;
  uint4 ubo_load_3 = s[scalar_offset_index_12 / 4];
  const int2 g = asint(((scalar_offset_index_12 & 2) ? ubo_load_3.zw : ubo_load_3.xy));
  const uint scalar_offset_bytes_13 = (((192u * idx) + 56u));
  const uint scalar_offset_index_13 = scalar_offset_bytes_13 / 4;
  uint4 ubo_load_4 = s[scalar_offset_index_13 / 4];
  const int2 h = asint(((scalar_offset_index_13 & 2) ? ubo_load_4.zw : ubo_load_4.xy));
  const float2x3 i = tint_symbol_9(s, ((192u * idx) + 64u));
  const float3x2 j = tint_symbol_10(s, ((192u * idx) + 96u));
  const int4 k[4] = tint_symbol_12(s, ((192u * idx) + 128u));
}

[numthreads(1, 1, 1)]
void main(tint_symbol_1 tint_symbol) {
  main_inner(tint_symbol.idx);
  return;
}
