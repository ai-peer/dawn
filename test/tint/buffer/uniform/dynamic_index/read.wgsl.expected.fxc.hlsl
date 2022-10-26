cbuffer cbuffer_ub : register(b0, space0) {
  uint4 ub[272];
};

struct tint_symbol_1 {
  uint idx : SV_GroupIndex;
};

float2x2 tint_symbol_14(uint4 buffer[272], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  uint4 ubo_load = buffer[scalar_offset_index / 4];
  const uint scalar_offset_bytes_1 = ((offset + 8u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  uint4 ubo_load_1 = buffer[scalar_offset_index_1 / 4];
  return float2x2(asfloat(((scalar_offset_index & 2) ? ubo_load.zw : ubo_load.xy)), asfloat(((scalar_offset_index_1 & 2) ? ubo_load_1.zw : ubo_load_1.xy)));
}

float2x3 tint_symbol_15(uint4 buffer[272], uint offset) {
  const uint scalar_offset_bytes_2 = ((offset + 0u));
  const uint scalar_offset_index_2 = scalar_offset_bytes_2 / 4;
  const uint scalar_offset_bytes_3 = ((offset + 16u));
  const uint scalar_offset_index_3 = scalar_offset_bytes_3 / 4;
  return float2x3(asfloat(buffer[scalar_offset_index_2 / 4].xyz), asfloat(buffer[scalar_offset_index_3 / 4].xyz));
}

float2x4 tint_symbol_16(uint4 buffer[272], uint offset) {
  const uint scalar_offset_bytes_4 = ((offset + 0u));
  const uint scalar_offset_index_4 = scalar_offset_bytes_4 / 4;
  const uint scalar_offset_bytes_5 = ((offset + 16u));
  const uint scalar_offset_index_5 = scalar_offset_bytes_5 / 4;
  return float2x4(asfloat(buffer[scalar_offset_index_4 / 4]), asfloat(buffer[scalar_offset_index_5 / 4]));
}

float3x2 tint_symbol_17(uint4 buffer[272], uint offset) {
  const uint scalar_offset_bytes_6 = ((offset + 0u));
  const uint scalar_offset_index_6 = scalar_offset_bytes_6 / 4;
  uint4 ubo_load_2 = buffer[scalar_offset_index_6 / 4];
  const uint scalar_offset_bytes_7 = ((offset + 8u));
  const uint scalar_offset_index_7 = scalar_offset_bytes_7 / 4;
  uint4 ubo_load_3 = buffer[scalar_offset_index_7 / 4];
  const uint scalar_offset_bytes_8 = ((offset + 16u));
  const uint scalar_offset_index_8 = scalar_offset_bytes_8 / 4;
  uint4 ubo_load_4 = buffer[scalar_offset_index_8 / 4];
  return float3x2(asfloat(((scalar_offset_index_6 & 2) ? ubo_load_2.zw : ubo_load_2.xy)), asfloat(((scalar_offset_index_7 & 2) ? ubo_load_3.zw : ubo_load_3.xy)), asfloat(((scalar_offset_index_8 & 2) ? ubo_load_4.zw : ubo_load_4.xy)));
}

float3x3 tint_symbol_18(uint4 buffer[272], uint offset) {
  const uint scalar_offset_bytes_9 = ((offset + 0u));
  const uint scalar_offset_index_9 = scalar_offset_bytes_9 / 4;
  const uint scalar_offset_bytes_10 = ((offset + 16u));
  const uint scalar_offset_index_10 = scalar_offset_bytes_10 / 4;
  const uint scalar_offset_bytes_11 = ((offset + 32u));
  const uint scalar_offset_index_11 = scalar_offset_bytes_11 / 4;
  return float3x3(asfloat(buffer[scalar_offset_index_9 / 4].xyz), asfloat(buffer[scalar_offset_index_10 / 4].xyz), asfloat(buffer[scalar_offset_index_11 / 4].xyz));
}

float3x4 tint_symbol_19(uint4 buffer[272], uint offset) {
  const uint scalar_offset_bytes_12 = ((offset + 0u));
  const uint scalar_offset_index_12 = scalar_offset_bytes_12 / 4;
  const uint scalar_offset_bytes_13 = ((offset + 16u));
  const uint scalar_offset_index_13 = scalar_offset_bytes_13 / 4;
  const uint scalar_offset_bytes_14 = ((offset + 32u));
  const uint scalar_offset_index_14 = scalar_offset_bytes_14 / 4;
  return float3x4(asfloat(buffer[scalar_offset_index_12 / 4]), asfloat(buffer[scalar_offset_index_13 / 4]), asfloat(buffer[scalar_offset_index_14 / 4]));
}

float4x2 tint_symbol_20(uint4 buffer[272], uint offset) {
  const uint scalar_offset_bytes_15 = ((offset + 0u));
  const uint scalar_offset_index_15 = scalar_offset_bytes_15 / 4;
  uint4 ubo_load_5 = buffer[scalar_offset_index_15 / 4];
  const uint scalar_offset_bytes_16 = ((offset + 8u));
  const uint scalar_offset_index_16 = scalar_offset_bytes_16 / 4;
  uint4 ubo_load_6 = buffer[scalar_offset_index_16 / 4];
  const uint scalar_offset_bytes_17 = ((offset + 16u));
  const uint scalar_offset_index_17 = scalar_offset_bytes_17 / 4;
  uint4 ubo_load_7 = buffer[scalar_offset_index_17 / 4];
  const uint scalar_offset_bytes_18 = ((offset + 24u));
  const uint scalar_offset_index_18 = scalar_offset_bytes_18 / 4;
  uint4 ubo_load_8 = buffer[scalar_offset_index_18 / 4];
  return float4x2(asfloat(((scalar_offset_index_15 & 2) ? ubo_load_5.zw : ubo_load_5.xy)), asfloat(((scalar_offset_index_16 & 2) ? ubo_load_6.zw : ubo_load_6.xy)), asfloat(((scalar_offset_index_17 & 2) ? ubo_load_7.zw : ubo_load_7.xy)), asfloat(((scalar_offset_index_18 & 2) ? ubo_load_8.zw : ubo_load_8.xy)));
}

float4x3 tint_symbol_21(uint4 buffer[272], uint offset) {
  const uint scalar_offset_bytes_19 = ((offset + 0u));
  const uint scalar_offset_index_19 = scalar_offset_bytes_19 / 4;
  const uint scalar_offset_bytes_20 = ((offset + 16u));
  const uint scalar_offset_index_20 = scalar_offset_bytes_20 / 4;
  const uint scalar_offset_bytes_21 = ((offset + 32u));
  const uint scalar_offset_index_21 = scalar_offset_bytes_21 / 4;
  const uint scalar_offset_bytes_22 = ((offset + 48u));
  const uint scalar_offset_index_22 = scalar_offset_bytes_22 / 4;
  return float4x3(asfloat(buffer[scalar_offset_index_19 / 4].xyz), asfloat(buffer[scalar_offset_index_20 / 4].xyz), asfloat(buffer[scalar_offset_index_21 / 4].xyz), asfloat(buffer[scalar_offset_index_22 / 4].xyz));
}

float4x4 tint_symbol_22(uint4 buffer[272], uint offset) {
  const uint scalar_offset_bytes_23 = ((offset + 0u));
  const uint scalar_offset_index_23 = scalar_offset_bytes_23 / 4;
  const uint scalar_offset_bytes_24 = ((offset + 16u));
  const uint scalar_offset_index_24 = scalar_offset_bytes_24 / 4;
  const uint scalar_offset_bytes_25 = ((offset + 32u));
  const uint scalar_offset_index_25 = scalar_offset_bytes_25 / 4;
  const uint scalar_offset_bytes_26 = ((offset + 48u));
  const uint scalar_offset_index_26 = scalar_offset_bytes_26 / 4;
  return float4x4(asfloat(buffer[scalar_offset_index_23 / 4]), asfloat(buffer[scalar_offset_index_24 / 4]), asfloat(buffer[scalar_offset_index_25 / 4]), asfloat(buffer[scalar_offset_index_26 / 4]));
}

typedef float3 tint_symbol_23_ret[2];
tint_symbol_23_ret tint_symbol_23(uint4 buffer[272], uint offset) {
  float3 arr_1[2] = (float3[2])0;
  {
    for(uint i = 0u; (i < 2u); i = (i + 1u)) {
      const uint scalar_offset_bytes_27 = ((offset + (i * 16u)));
      const uint scalar_offset_index_27 = scalar_offset_bytes_27 / 4;
      arr_1[i] = asfloat(buffer[scalar_offset_index_27 / 4].xyz);
    }
  }
  return arr_1;
}

void main_inner(uint idx) {
  const uint scalar_offset_bytes_28 = ((544u * idx));
  const uint scalar_offset_index_28 = scalar_offset_bytes_28 / 4;
  const float scalar_f32 = asfloat(ub[scalar_offset_index_28 / 4][scalar_offset_index_28 % 4]);
  const uint scalar_offset_bytes_29 = (((544u * idx) + 4u));
  const uint scalar_offset_index_29 = scalar_offset_bytes_29 / 4;
  const int scalar_i32 = asint(ub[scalar_offset_index_29 / 4][scalar_offset_index_29 % 4]);
  const uint scalar_offset_bytes_30 = (((544u * idx) + 8u));
  const uint scalar_offset_index_30 = scalar_offset_bytes_30 / 4;
  const uint scalar_u32 = ub[scalar_offset_index_30 / 4][scalar_offset_index_30 % 4];
  const uint scalar_offset_bytes_31 = (((544u * idx) + 16u));
  const uint scalar_offset_index_31 = scalar_offset_bytes_31 / 4;
  uint4 ubo_load_9 = ub[scalar_offset_index_31 / 4];
  const float2 vec2_f32 = asfloat(((scalar_offset_index_31 & 2) ? ubo_load_9.zw : ubo_load_9.xy));
  const uint scalar_offset_bytes_32 = (((544u * idx) + 24u));
  const uint scalar_offset_index_32 = scalar_offset_bytes_32 / 4;
  uint4 ubo_load_10 = ub[scalar_offset_index_32 / 4];
  const int2 vec2_i32 = asint(((scalar_offset_index_32 & 2) ? ubo_load_10.zw : ubo_load_10.xy));
  const uint scalar_offset_bytes_33 = (((544u * idx) + 32u));
  const uint scalar_offset_index_33 = scalar_offset_bytes_33 / 4;
  uint4 ubo_load_11 = ub[scalar_offset_index_33 / 4];
  const uint2 vec2_u32 = ((scalar_offset_index_33 & 2) ? ubo_load_11.zw : ubo_load_11.xy);
  const uint scalar_offset_bytes_34 = (((544u * idx) + 48u));
  const uint scalar_offset_index_34 = scalar_offset_bytes_34 / 4;
  const float3 vec3_f32 = asfloat(ub[scalar_offset_index_34 / 4].xyz);
  const uint scalar_offset_bytes_35 = (((544u * idx) + 64u));
  const uint scalar_offset_index_35 = scalar_offset_bytes_35 / 4;
  const int3 vec3_i32 = asint(ub[scalar_offset_index_35 / 4].xyz);
  const uint scalar_offset_bytes_36 = (((544u * idx) + 80u));
  const uint scalar_offset_index_36 = scalar_offset_bytes_36 / 4;
  const uint3 vec3_u32 = ub[scalar_offset_index_36 / 4].xyz;
  const uint scalar_offset_bytes_37 = (((544u * idx) + 96u));
  const uint scalar_offset_index_37 = scalar_offset_bytes_37 / 4;
  const float4 vec4_f32 = asfloat(ub[scalar_offset_index_37 / 4]);
  const uint scalar_offset_bytes_38 = (((544u * idx) + 112u));
  const uint scalar_offset_index_38 = scalar_offset_bytes_38 / 4;
  const int4 vec4_i32 = asint(ub[scalar_offset_index_38 / 4]);
  const uint scalar_offset_bytes_39 = (((544u * idx) + 128u));
  const uint scalar_offset_index_39 = scalar_offset_bytes_39 / 4;
  const uint4 vec4_u32 = ub[scalar_offset_index_39 / 4];
  const float2x2 mat2x2_f32 = tint_symbol_14(ub, ((544u * idx) + 144u));
  const float2x3 mat2x3_f32 = tint_symbol_15(ub, ((544u * idx) + 160u));
  const float2x4 mat2x4_f32 = tint_symbol_16(ub, ((544u * idx) + 192u));
  const float3x2 mat3x2_f32 = tint_symbol_17(ub, ((544u * idx) + 224u));
  const float3x3 mat3x3_f32 = tint_symbol_18(ub, ((544u * idx) + 256u));
  const float3x4 mat3x4_f32 = tint_symbol_19(ub, ((544u * idx) + 304u));
  const float4x2 mat4x2_f32 = tint_symbol_20(ub, ((544u * idx) + 352u));
  const float4x3 mat4x3_f32 = tint_symbol_21(ub, ((544u * idx) + 384u));
  const float4x4 mat4x4_f32 = tint_symbol_22(ub, ((544u * idx) + 448u));
  const float3 arr2_vec3_f32[2] = tint_symbol_23(ub, ((544u * idx) + 512u));
}

[numthreads(1, 1, 1)]
void main(tint_symbol_1 tint_symbol) {
  main_inner(tint_symbol.idx);
  return;
}
