SKIP: FAILED

cbuffer cbuffer_ub : register(b0, space0) {
  uint4 ub[400];
};

struct tint_symbol_1 {
  uint idx : SV_GroupIndex;
};

float2x2 tint_symbol_18(uint4 buffer[400], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  uint4 ubo_load = buffer[scalar_offset_index / 4];
  const uint scalar_offset_bytes_1 = ((offset + 8u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  uint4 ubo_load_1 = buffer[scalar_offset_index_1 / 4];
  return float2x2(asfloat(((scalar_offset_index & 2) ? ubo_load.zw : ubo_load.xy)), asfloat(((scalar_offset_index_1 & 2) ? ubo_load_1.zw : ubo_load_1.xy)));
}

float2x3 tint_symbol_19(uint4 buffer[400], uint offset) {
  const uint scalar_offset_bytes_2 = ((offset + 0u));
  const uint scalar_offset_index_2 = scalar_offset_bytes_2 / 4;
  const uint scalar_offset_bytes_3 = ((offset + 16u));
  const uint scalar_offset_index_3 = scalar_offset_bytes_3 / 4;
  return float2x3(asfloat(buffer[scalar_offset_index_2 / 4].xyz), asfloat(buffer[scalar_offset_index_3 / 4].xyz));
}

float2x4 tint_symbol_20(uint4 buffer[400], uint offset) {
  const uint scalar_offset_bytes_4 = ((offset + 0u));
  const uint scalar_offset_index_4 = scalar_offset_bytes_4 / 4;
  const uint scalar_offset_bytes_5 = ((offset + 16u));
  const uint scalar_offset_index_5 = scalar_offset_bytes_5 / 4;
  return float2x4(asfloat(buffer[scalar_offset_index_4 / 4]), asfloat(buffer[scalar_offset_index_5 / 4]));
}

float3x2 tint_symbol_21(uint4 buffer[400], uint offset) {
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

float3x3 tint_symbol_22(uint4 buffer[400], uint offset) {
  const uint scalar_offset_bytes_9 = ((offset + 0u));
  const uint scalar_offset_index_9 = scalar_offset_bytes_9 / 4;
  const uint scalar_offset_bytes_10 = ((offset + 16u));
  const uint scalar_offset_index_10 = scalar_offset_bytes_10 / 4;
  const uint scalar_offset_bytes_11 = ((offset + 32u));
  const uint scalar_offset_index_11 = scalar_offset_bytes_11 / 4;
  return float3x3(asfloat(buffer[scalar_offset_index_9 / 4].xyz), asfloat(buffer[scalar_offset_index_10 / 4].xyz), asfloat(buffer[scalar_offset_index_11 / 4].xyz));
}

float3x4 tint_symbol_23(uint4 buffer[400], uint offset) {
  const uint scalar_offset_bytes_12 = ((offset + 0u));
  const uint scalar_offset_index_12 = scalar_offset_bytes_12 / 4;
  const uint scalar_offset_bytes_13 = ((offset + 16u));
  const uint scalar_offset_index_13 = scalar_offset_bytes_13 / 4;
  const uint scalar_offset_bytes_14 = ((offset + 32u));
  const uint scalar_offset_index_14 = scalar_offset_bytes_14 / 4;
  return float3x4(asfloat(buffer[scalar_offset_index_12 / 4]), asfloat(buffer[scalar_offset_index_13 / 4]), asfloat(buffer[scalar_offset_index_14 / 4]));
}

float4x2 tint_symbol_24(uint4 buffer[400], uint offset) {
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

float4x3 tint_symbol_25(uint4 buffer[400], uint offset) {
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

float4x4 tint_symbol_26(uint4 buffer[400], uint offset) {
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

matrix<float16_t, 2, 2> tint_symbol_27(uint4 buffer[400], uint offset) {
  const uint scalar_offset_bytes_27 = ((offset + 0u));
  const uint scalar_offset_index_27 = scalar_offset_bytes_27 / 4;
  uint ubo_load_9 = buffer[scalar_offset_index_27 / 4][scalar_offset_index_27 % 4];
  const uint scalar_offset_bytes_28 = ((offset + 4u));
  const uint scalar_offset_index_28 = scalar_offset_bytes_28 / 4;
  uint ubo_load_10 = buffer[scalar_offset_index_28 / 4][scalar_offset_index_28 % 4];
  return matrix<float16_t, 2, 2>(vector<float16_t, 2>(float16_t(f16tof32(ubo_load_9 & 0xFFFF)), float16_t(f16tof32(ubo_load_9 >> 16))), vector<float16_t, 2>(float16_t(f16tof32(ubo_load_10 & 0xFFFF)), float16_t(f16tof32(ubo_load_10 >> 16))));
}

matrix<float16_t, 2, 3> tint_symbol_28(uint4 buffer[400], uint offset) {
  const uint scalar_offset_bytes_29 = ((offset + 0u));
  const uint scalar_offset_index_29 = scalar_offset_bytes_29 / 4;
  uint4 ubo_load_12 = buffer[scalar_offset_index_29 / 4];
  uint2 ubo_load_11 = ((scalar_offset_index_29 & 2) ? ubo_load_12.zw : ubo_load_12.xy);
  vector<float16_t, 2> ubo_load_11_xz = vector<float16_t, 2>(f16tof32(ubo_load_11 & 0xFFFF));
  float16_t ubo_load_11_y = f16tof32(ubo_load_11[0] >> 16);
  const uint scalar_offset_bytes_30 = ((offset + 8u));
  const uint scalar_offset_index_30 = scalar_offset_bytes_30 / 4;
  uint4 ubo_load_14 = buffer[scalar_offset_index_30 / 4];
  uint2 ubo_load_13 = ((scalar_offset_index_30 & 2) ? ubo_load_14.zw : ubo_load_14.xy);
  vector<float16_t, 2> ubo_load_13_xz = vector<float16_t, 2>(f16tof32(ubo_load_13 & 0xFFFF));
  float16_t ubo_load_13_y = f16tof32(ubo_load_13[0] >> 16);
  return matrix<float16_t, 2, 3>(vector<float16_t, 3>(ubo_load_11_xz[0], ubo_load_11_y, ubo_load_11_xz[1]), vector<float16_t, 3>(ubo_load_13_xz[0], ubo_load_13_y, ubo_load_13_xz[1]));
}

matrix<float16_t, 2, 4> tint_symbol_29(uint4 buffer[400], uint offset) {
  const uint scalar_offset_bytes_31 = ((offset + 0u));
  const uint scalar_offset_index_31 = scalar_offset_bytes_31 / 4;
  uint4 ubo_load_16 = buffer[scalar_offset_index_31 / 4];
  uint2 ubo_load_15 = ((scalar_offset_index_31 & 2) ? ubo_load_16.zw : ubo_load_16.xy);
  vector<float16_t, 2> ubo_load_15_xz = vector<float16_t, 2>(f16tof32(ubo_load_15 & 0xFFFF));
  vector<float16_t, 2> ubo_load_15_yw = vector<float16_t, 2>(f16tof32(ubo_load_15 >> 16));
  const uint scalar_offset_bytes_32 = ((offset + 8u));
  const uint scalar_offset_index_32 = scalar_offset_bytes_32 / 4;
  uint4 ubo_load_18 = buffer[scalar_offset_index_32 / 4];
  uint2 ubo_load_17 = ((scalar_offset_index_32 & 2) ? ubo_load_18.zw : ubo_load_18.xy);
  vector<float16_t, 2> ubo_load_17_xz = vector<float16_t, 2>(f16tof32(ubo_load_17 & 0xFFFF));
  vector<float16_t, 2> ubo_load_17_yw = vector<float16_t, 2>(f16tof32(ubo_load_17 >> 16));
  return matrix<float16_t, 2, 4>(vector<float16_t, 4>(ubo_load_15_xz[0], ubo_load_15_yw[0], ubo_load_15_xz[1], ubo_load_15_yw[1]), vector<float16_t, 4>(ubo_load_17_xz[0], ubo_load_17_yw[0], ubo_load_17_xz[1], ubo_load_17_yw[1]));
}

matrix<float16_t, 3, 2> tint_symbol_30(uint4 buffer[400], uint offset) {
  const uint scalar_offset_bytes_33 = ((offset + 0u));
  const uint scalar_offset_index_33 = scalar_offset_bytes_33 / 4;
  uint ubo_load_19 = buffer[scalar_offset_index_33 / 4][scalar_offset_index_33 % 4];
  const uint scalar_offset_bytes_34 = ((offset + 4u));
  const uint scalar_offset_index_34 = scalar_offset_bytes_34 / 4;
  uint ubo_load_20 = buffer[scalar_offset_index_34 / 4][scalar_offset_index_34 % 4];
  const uint scalar_offset_bytes_35 = ((offset + 8u));
  const uint scalar_offset_index_35 = scalar_offset_bytes_35 / 4;
  uint ubo_load_21 = buffer[scalar_offset_index_35 / 4][scalar_offset_index_35 % 4];
  return matrix<float16_t, 3, 2>(vector<float16_t, 2>(float16_t(f16tof32(ubo_load_19 & 0xFFFF)), float16_t(f16tof32(ubo_load_19 >> 16))), vector<float16_t, 2>(float16_t(f16tof32(ubo_load_20 & 0xFFFF)), float16_t(f16tof32(ubo_load_20 >> 16))), vector<float16_t, 2>(float16_t(f16tof32(ubo_load_21 & 0xFFFF)), float16_t(f16tof32(ubo_load_21 >> 16))));
}

matrix<float16_t, 3, 3> tint_symbol_31(uint4 buffer[400], uint offset) {
  const uint scalar_offset_bytes_36 = ((offset + 0u));
  const uint scalar_offset_index_36 = scalar_offset_bytes_36 / 4;
  uint4 ubo_load_23 = buffer[scalar_offset_index_36 / 4];
  uint2 ubo_load_22 = ((scalar_offset_index_36 & 2) ? ubo_load_23.zw : ubo_load_23.xy);
  vector<float16_t, 2> ubo_load_22_xz = vector<float16_t, 2>(f16tof32(ubo_load_22 & 0xFFFF));
  float16_t ubo_load_22_y = f16tof32(ubo_load_22[0] >> 16);
  const uint scalar_offset_bytes_37 = ((offset + 8u));
  const uint scalar_offset_index_37 = scalar_offset_bytes_37 / 4;
  uint4 ubo_load_25 = buffer[scalar_offset_index_37 / 4];
  uint2 ubo_load_24 = ((scalar_offset_index_37 & 2) ? ubo_load_25.zw : ubo_load_25.xy);
  vector<float16_t, 2> ubo_load_24_xz = vector<float16_t, 2>(f16tof32(ubo_load_24 & 0xFFFF));
  float16_t ubo_load_24_y = f16tof32(ubo_load_24[0] >> 16);
  const uint scalar_offset_bytes_38 = ((offset + 16u));
  const uint scalar_offset_index_38 = scalar_offset_bytes_38 / 4;
  uint4 ubo_load_27 = buffer[scalar_offset_index_38 / 4];
  uint2 ubo_load_26 = ((scalar_offset_index_38 & 2) ? ubo_load_27.zw : ubo_load_27.xy);
  vector<float16_t, 2> ubo_load_26_xz = vector<float16_t, 2>(f16tof32(ubo_load_26 & 0xFFFF));
  float16_t ubo_load_26_y = f16tof32(ubo_load_26[0] >> 16);
  return matrix<float16_t, 3, 3>(vector<float16_t, 3>(ubo_load_22_xz[0], ubo_load_22_y, ubo_load_22_xz[1]), vector<float16_t, 3>(ubo_load_24_xz[0], ubo_load_24_y, ubo_load_24_xz[1]), vector<float16_t, 3>(ubo_load_26_xz[0], ubo_load_26_y, ubo_load_26_xz[1]));
}

matrix<float16_t, 3, 4> tint_symbol_32(uint4 buffer[400], uint offset) {
  const uint scalar_offset_bytes_39 = ((offset + 0u));
  const uint scalar_offset_index_39 = scalar_offset_bytes_39 / 4;
  uint4 ubo_load_29 = buffer[scalar_offset_index_39 / 4];
  uint2 ubo_load_28 = ((scalar_offset_index_39 & 2) ? ubo_load_29.zw : ubo_load_29.xy);
  vector<float16_t, 2> ubo_load_28_xz = vector<float16_t, 2>(f16tof32(ubo_load_28 & 0xFFFF));
  vector<float16_t, 2> ubo_load_28_yw = vector<float16_t, 2>(f16tof32(ubo_load_28 >> 16));
  const uint scalar_offset_bytes_40 = ((offset + 8u));
  const uint scalar_offset_index_40 = scalar_offset_bytes_40 / 4;
  uint4 ubo_load_31 = buffer[scalar_offset_index_40 / 4];
  uint2 ubo_load_30 = ((scalar_offset_index_40 & 2) ? ubo_load_31.zw : ubo_load_31.xy);
  vector<float16_t, 2> ubo_load_30_xz = vector<float16_t, 2>(f16tof32(ubo_load_30 & 0xFFFF));
  vector<float16_t, 2> ubo_load_30_yw = vector<float16_t, 2>(f16tof32(ubo_load_30 >> 16));
  const uint scalar_offset_bytes_41 = ((offset + 16u));
  const uint scalar_offset_index_41 = scalar_offset_bytes_41 / 4;
  uint4 ubo_load_33 = buffer[scalar_offset_index_41 / 4];
  uint2 ubo_load_32 = ((scalar_offset_index_41 & 2) ? ubo_load_33.zw : ubo_load_33.xy);
  vector<float16_t, 2> ubo_load_32_xz = vector<float16_t, 2>(f16tof32(ubo_load_32 & 0xFFFF));
  vector<float16_t, 2> ubo_load_32_yw = vector<float16_t, 2>(f16tof32(ubo_load_32 >> 16));
  return matrix<float16_t, 3, 4>(vector<float16_t, 4>(ubo_load_28_xz[0], ubo_load_28_yw[0], ubo_load_28_xz[1], ubo_load_28_yw[1]), vector<float16_t, 4>(ubo_load_30_xz[0], ubo_load_30_yw[0], ubo_load_30_xz[1], ubo_load_30_yw[1]), vector<float16_t, 4>(ubo_load_32_xz[0], ubo_load_32_yw[0], ubo_load_32_xz[1], ubo_load_32_yw[1]));
}

matrix<float16_t, 4, 2> tint_symbol_33(uint4 buffer[400], uint offset) {
  const uint scalar_offset_bytes_42 = ((offset + 0u));
  const uint scalar_offset_index_42 = scalar_offset_bytes_42 / 4;
  uint ubo_load_34 = buffer[scalar_offset_index_42 / 4][scalar_offset_index_42 % 4];
  const uint scalar_offset_bytes_43 = ((offset + 4u));
  const uint scalar_offset_index_43 = scalar_offset_bytes_43 / 4;
  uint ubo_load_35 = buffer[scalar_offset_index_43 / 4][scalar_offset_index_43 % 4];
  const uint scalar_offset_bytes_44 = ((offset + 8u));
  const uint scalar_offset_index_44 = scalar_offset_bytes_44 / 4;
  uint ubo_load_36 = buffer[scalar_offset_index_44 / 4][scalar_offset_index_44 % 4];
  const uint scalar_offset_bytes_45 = ((offset + 12u));
  const uint scalar_offset_index_45 = scalar_offset_bytes_45 / 4;
  uint ubo_load_37 = buffer[scalar_offset_index_45 / 4][scalar_offset_index_45 % 4];
  return matrix<float16_t, 4, 2>(vector<float16_t, 2>(float16_t(f16tof32(ubo_load_34 & 0xFFFF)), float16_t(f16tof32(ubo_load_34 >> 16))), vector<float16_t, 2>(float16_t(f16tof32(ubo_load_35 & 0xFFFF)), float16_t(f16tof32(ubo_load_35 >> 16))), vector<float16_t, 2>(float16_t(f16tof32(ubo_load_36 & 0xFFFF)), float16_t(f16tof32(ubo_load_36 >> 16))), vector<float16_t, 2>(float16_t(f16tof32(ubo_load_37 & 0xFFFF)), float16_t(f16tof32(ubo_load_37 >> 16))));
}

matrix<float16_t, 4, 3> tint_symbol_34(uint4 buffer[400], uint offset) {
  const uint scalar_offset_bytes_46 = ((offset + 0u));
  const uint scalar_offset_index_46 = scalar_offset_bytes_46 / 4;
  uint4 ubo_load_39 = buffer[scalar_offset_index_46 / 4];
  uint2 ubo_load_38 = ((scalar_offset_index_46 & 2) ? ubo_load_39.zw : ubo_load_39.xy);
  vector<float16_t, 2> ubo_load_38_xz = vector<float16_t, 2>(f16tof32(ubo_load_38 & 0xFFFF));
  float16_t ubo_load_38_y = f16tof32(ubo_load_38[0] >> 16);
  const uint scalar_offset_bytes_47 = ((offset + 8u));
  const uint scalar_offset_index_47 = scalar_offset_bytes_47 / 4;
  uint4 ubo_load_41 = buffer[scalar_offset_index_47 / 4];
  uint2 ubo_load_40 = ((scalar_offset_index_47 & 2) ? ubo_load_41.zw : ubo_load_41.xy);
  vector<float16_t, 2> ubo_load_40_xz = vector<float16_t, 2>(f16tof32(ubo_load_40 & 0xFFFF));
  float16_t ubo_load_40_y = f16tof32(ubo_load_40[0] >> 16);
  const uint scalar_offset_bytes_48 = ((offset + 16u));
  const uint scalar_offset_index_48 = scalar_offset_bytes_48 / 4;
  uint4 ubo_load_43 = buffer[scalar_offset_index_48 / 4];
  uint2 ubo_load_42 = ((scalar_offset_index_48 & 2) ? ubo_load_43.zw : ubo_load_43.xy);
  vector<float16_t, 2> ubo_load_42_xz = vector<float16_t, 2>(f16tof32(ubo_load_42 & 0xFFFF));
  float16_t ubo_load_42_y = f16tof32(ubo_load_42[0] >> 16);
  const uint scalar_offset_bytes_49 = ((offset + 24u));
  const uint scalar_offset_index_49 = scalar_offset_bytes_49 / 4;
  uint4 ubo_load_45 = buffer[scalar_offset_index_49 / 4];
  uint2 ubo_load_44 = ((scalar_offset_index_49 & 2) ? ubo_load_45.zw : ubo_load_45.xy);
  vector<float16_t, 2> ubo_load_44_xz = vector<float16_t, 2>(f16tof32(ubo_load_44 & 0xFFFF));
  float16_t ubo_load_44_y = f16tof32(ubo_load_44[0] >> 16);
  return matrix<float16_t, 4, 3>(vector<float16_t, 3>(ubo_load_38_xz[0], ubo_load_38_y, ubo_load_38_xz[1]), vector<float16_t, 3>(ubo_load_40_xz[0], ubo_load_40_y, ubo_load_40_xz[1]), vector<float16_t, 3>(ubo_load_42_xz[0], ubo_load_42_y, ubo_load_42_xz[1]), vector<float16_t, 3>(ubo_load_44_xz[0], ubo_load_44_y, ubo_load_44_xz[1]));
}

matrix<float16_t, 4, 4> tint_symbol_35(uint4 buffer[400], uint offset) {
  const uint scalar_offset_bytes_50 = ((offset + 0u));
  const uint scalar_offset_index_50 = scalar_offset_bytes_50 / 4;
  uint4 ubo_load_47 = buffer[scalar_offset_index_50 / 4];
  uint2 ubo_load_46 = ((scalar_offset_index_50 & 2) ? ubo_load_47.zw : ubo_load_47.xy);
  vector<float16_t, 2> ubo_load_46_xz = vector<float16_t, 2>(f16tof32(ubo_load_46 & 0xFFFF));
  vector<float16_t, 2> ubo_load_46_yw = vector<float16_t, 2>(f16tof32(ubo_load_46 >> 16));
  const uint scalar_offset_bytes_51 = ((offset + 8u));
  const uint scalar_offset_index_51 = scalar_offset_bytes_51 / 4;
  uint4 ubo_load_49 = buffer[scalar_offset_index_51 / 4];
  uint2 ubo_load_48 = ((scalar_offset_index_51 & 2) ? ubo_load_49.zw : ubo_load_49.xy);
  vector<float16_t, 2> ubo_load_48_xz = vector<float16_t, 2>(f16tof32(ubo_load_48 & 0xFFFF));
  vector<float16_t, 2> ubo_load_48_yw = vector<float16_t, 2>(f16tof32(ubo_load_48 >> 16));
  const uint scalar_offset_bytes_52 = ((offset + 16u));
  const uint scalar_offset_index_52 = scalar_offset_bytes_52 / 4;
  uint4 ubo_load_51 = buffer[scalar_offset_index_52 / 4];
  uint2 ubo_load_50 = ((scalar_offset_index_52 & 2) ? ubo_load_51.zw : ubo_load_51.xy);
  vector<float16_t, 2> ubo_load_50_xz = vector<float16_t, 2>(f16tof32(ubo_load_50 & 0xFFFF));
  vector<float16_t, 2> ubo_load_50_yw = vector<float16_t, 2>(f16tof32(ubo_load_50 >> 16));
  const uint scalar_offset_bytes_53 = ((offset + 24u));
  const uint scalar_offset_index_53 = scalar_offset_bytes_53 / 4;
  uint4 ubo_load_53 = buffer[scalar_offset_index_53 / 4];
  uint2 ubo_load_52 = ((scalar_offset_index_53 & 2) ? ubo_load_53.zw : ubo_load_53.xy);
  vector<float16_t, 2> ubo_load_52_xz = vector<float16_t, 2>(f16tof32(ubo_load_52 & 0xFFFF));
  vector<float16_t, 2> ubo_load_52_yw = vector<float16_t, 2>(f16tof32(ubo_load_52 >> 16));
  return matrix<float16_t, 4, 4>(vector<float16_t, 4>(ubo_load_46_xz[0], ubo_load_46_yw[0], ubo_load_46_xz[1], ubo_load_46_yw[1]), vector<float16_t, 4>(ubo_load_48_xz[0], ubo_load_48_yw[0], ubo_load_48_xz[1], ubo_load_48_yw[1]), vector<float16_t, 4>(ubo_load_50_xz[0], ubo_load_50_yw[0], ubo_load_50_xz[1], ubo_load_50_yw[1]), vector<float16_t, 4>(ubo_load_52_xz[0], ubo_load_52_yw[0], ubo_load_52_xz[1], ubo_load_52_yw[1]));
}

typedef float3 tint_symbol_36_ret[2];
tint_symbol_36_ret tint_symbol_36(uint4 buffer[400], uint offset) {
  float3 arr_1[2] = (float3[2])0;
  {
    for(uint i = 0u; (i < 2u); i = (i + 1u)) {
      const uint scalar_offset_bytes_54 = ((offset + (i * 16u)));
      const uint scalar_offset_index_54 = scalar_offset_bytes_54 / 4;
      arr_1[i] = asfloat(buffer[scalar_offset_index_54 / 4].xyz);
    }
  }
  return arr_1;
}

typedef matrix<float16_t, 4, 2> tint_symbol_37_ret[2];
tint_symbol_37_ret tint_symbol_37(uint4 buffer[400], uint offset) {
  matrix<float16_t, 4, 2> arr_2[2] = (matrix<float16_t, 4, 2>[2])0;
  {
    for(uint i_1 = 0u; (i_1 < 2u); i_1 = (i_1 + 1u)) {
      arr_2[i_1] = tint_symbol_33(buffer, (offset + (i_1 * 16u)));
    }
  }
  return arr_2;
}

void main_inner(uint idx) {
  const uint scalar_offset_bytes_55 = ((800u * idx));
  const uint scalar_offset_index_55 = scalar_offset_bytes_55 / 4;
  const float scalar_f32 = asfloat(ub[scalar_offset_index_55 / 4][scalar_offset_index_55 % 4]);
  const uint scalar_offset_bytes_56 = (((800u * idx) + 4u));
  const uint scalar_offset_index_56 = scalar_offset_bytes_56 / 4;
  const int scalar_i32 = asint(ub[scalar_offset_index_56 / 4][scalar_offset_index_56 % 4]);
  const uint scalar_offset_bytes_57 = (((800u * idx) + 8u));
  const uint scalar_offset_index_57 = scalar_offset_bytes_57 / 4;
  const uint scalar_u32 = ub[scalar_offset_index_57 / 4][scalar_offset_index_57 % 4];
  const uint scalar_offset_bytes_58 = (((800u * idx) + 12u));
  const uint scalar_offset_index_58 = scalar_offset_bytes_58 / 4;
  const float16_t scalar_f16 = float16_t(f16tof32(((ub[scalar_offset_index_58 / 4][scalar_offset_index_58 % 4] >> (scalar_offset_bytes_58 % 4 == 0 ? 0 : 16)) & 0xFFFF)));
  const uint scalar_offset_bytes_59 = (((800u * idx) + 16u));
  const uint scalar_offset_index_59 = scalar_offset_bytes_59 / 4;
  uint4 ubo_load_54 = ub[scalar_offset_index_59 / 4];
  const float2 vec2_f32 = asfloat(((scalar_offset_index_59 & 2) ? ubo_load_54.zw : ubo_load_54.xy));
  const uint scalar_offset_bytes_60 = (((800u * idx) + 24u));
  const uint scalar_offset_index_60 = scalar_offset_bytes_60 / 4;
  uint4 ubo_load_55 = ub[scalar_offset_index_60 / 4];
  const int2 vec2_i32 = asint(((scalar_offset_index_60 & 2) ? ubo_load_55.zw : ubo_load_55.xy));
  const uint scalar_offset_bytes_61 = (((800u * idx) + 32u));
  const uint scalar_offset_index_61 = scalar_offset_bytes_61 / 4;
  uint4 ubo_load_56 = ub[scalar_offset_index_61 / 4];
  const uint2 vec2_u32 = ((scalar_offset_index_61 & 2) ? ubo_load_56.zw : ubo_load_56.xy);
  const uint scalar_offset_bytes_62 = (((800u * idx) + 40u));
  const uint scalar_offset_index_62 = scalar_offset_bytes_62 / 4;
  uint ubo_load_57 = ub[scalar_offset_index_62 / 4][scalar_offset_index_62 % 4];
  const vector<float16_t, 2> vec2_f16 = vector<float16_t, 2>(float16_t(f16tof32(ubo_load_57 & 0xFFFF)), float16_t(f16tof32(ubo_load_57 >> 16)));
  const uint scalar_offset_bytes_63 = (((800u * idx) + 48u));
  const uint scalar_offset_index_63 = scalar_offset_bytes_63 / 4;
  const float3 vec3_f32 = asfloat(ub[scalar_offset_index_63 / 4].xyz);
  const uint scalar_offset_bytes_64 = (((800u * idx) + 64u));
  const uint scalar_offset_index_64 = scalar_offset_bytes_64 / 4;
  const int3 vec3_i32 = asint(ub[scalar_offset_index_64 / 4].xyz);
  const uint scalar_offset_bytes_65 = (((800u * idx) + 80u));
  const uint scalar_offset_index_65 = scalar_offset_bytes_65 / 4;
  const uint3 vec3_u32 = ub[scalar_offset_index_65 / 4].xyz;
  const uint scalar_offset_bytes_66 = (((800u * idx) + 96u));
  const uint scalar_offset_index_66 = scalar_offset_bytes_66 / 4;
  uint4 ubo_load_59 = ub[scalar_offset_index_66 / 4];
  uint2 ubo_load_58 = ((scalar_offset_index_66 & 2) ? ubo_load_59.zw : ubo_load_59.xy);
  vector<float16_t, 2> ubo_load_58_xz = vector<float16_t, 2>(f16tof32(ubo_load_58 & 0xFFFF));
  float16_t ubo_load_58_y = f16tof32(ubo_load_58[0] >> 16);
  const vector<float16_t, 3> vec3_f16 = vector<float16_t, 3>(ubo_load_58_xz[0], ubo_load_58_y, ubo_load_58_xz[1]);
  const uint scalar_offset_bytes_67 = (((800u * idx) + 112u));
  const uint scalar_offset_index_67 = scalar_offset_bytes_67 / 4;
  const float4 vec4_f32 = asfloat(ub[scalar_offset_index_67 / 4]);
  const uint scalar_offset_bytes_68 = (((800u * idx) + 128u));
  const uint scalar_offset_index_68 = scalar_offset_bytes_68 / 4;
  const int4 vec4_i32 = asint(ub[scalar_offset_index_68 / 4]);
  const uint scalar_offset_bytes_69 = (((800u * idx) + 144u));
  const uint scalar_offset_index_69 = scalar_offset_bytes_69 / 4;
  const uint4 vec4_u32 = ub[scalar_offset_index_69 / 4];
  const uint scalar_offset_bytes_70 = (((800u * idx) + 160u));
  const uint scalar_offset_index_70 = scalar_offset_bytes_70 / 4;
  uint4 ubo_load_61 = ub[scalar_offset_index_70 / 4];
  uint2 ubo_load_60 = ((scalar_offset_index_70 & 2) ? ubo_load_61.zw : ubo_load_61.xy);
  vector<float16_t, 2> ubo_load_60_xz = vector<float16_t, 2>(f16tof32(ubo_load_60 & 0xFFFF));
  vector<float16_t, 2> ubo_load_60_yw = vector<float16_t, 2>(f16tof32(ubo_load_60 >> 16));
  const vector<float16_t, 4> vec4_f16 = vector<float16_t, 4>(ubo_load_60_xz[0], ubo_load_60_yw[0], ubo_load_60_xz[1], ubo_load_60_yw[1]);
  const float2x2 mat2x2_f32 = tint_symbol_18(ub, ((800u * idx) + 168u));
  const float2x3 mat2x3_f32 = tint_symbol_19(ub, ((800u * idx) + 192u));
  const float2x4 mat2x4_f32 = tint_symbol_20(ub, ((800u * idx) + 224u));
  const float3x2 mat3x2_f32 = tint_symbol_21(ub, ((800u * idx) + 256u));
  const float3x3 mat3x3_f32 = tint_symbol_22(ub, ((800u * idx) + 288u));
  const float3x4 mat3x4_f32 = tint_symbol_23(ub, ((800u * idx) + 336u));
  const float4x2 mat4x2_f32 = tint_symbol_24(ub, ((800u * idx) + 384u));
  const float4x3 mat4x3_f32 = tint_symbol_25(ub, ((800u * idx) + 416u));
  const float4x4 mat4x4_f32 = tint_symbol_26(ub, ((800u * idx) + 480u));
  const matrix<float16_t, 2, 2> mat2x2_f16 = tint_symbol_27(ub, ((800u * idx) + 544u));
  const matrix<float16_t, 2, 3> mat2x3_f16 = tint_symbol_28(ub, ((800u * idx) + 552u));
  const matrix<float16_t, 2, 4> mat2x4_f16 = tint_symbol_29(ub, ((800u * idx) + 568u));
  const matrix<float16_t, 3, 2> mat3x2_f16 = tint_symbol_30(ub, ((800u * idx) + 584u));
  const matrix<float16_t, 3, 3> mat3x3_f16 = tint_symbol_31(ub, ((800u * idx) + 600u));
  const matrix<float16_t, 3, 4> mat3x4_f16 = tint_symbol_32(ub, ((800u * idx) + 624u));
  const matrix<float16_t, 4, 2> mat4x2_f16 = tint_symbol_33(ub, ((800u * idx) + 648u));
  const matrix<float16_t, 4, 3> mat4x3_f16 = tint_symbol_34(ub, ((800u * idx) + 664u));
  const matrix<float16_t, 4, 4> mat4x4_f16 = tint_symbol_35(ub, ((800u * idx) + 696u));
  const float3 arr2_vec3_f32[2] = tint_symbol_36(ub, ((800u * idx) + 736u));
  const matrix<float16_t, 4, 2> arr2_mat4x2_f16[2] = tint_symbol_37(ub, ((800u * idx) + 768u));
}

[numthreads(1, 1, 1)]
void main(tint_symbol_1 tint_symbol) {
  main_inner(tint_symbol.idx);
  return;
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\buffer\Shader@0x0000025DC63FBAA0(108,8-16): error X3000: syntax error: unexpected token 'float16_t'

