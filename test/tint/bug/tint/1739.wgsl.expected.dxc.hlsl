struct GammaTransferParams {
  float G;
  float A;
  float B;
  float C;
  float D;
  float E;
  float F;
  uint padding;
};
struct ExternalTextureParams {
  uint numPlanes;
  uint doYuvToRgbConversionOnly;
  float3x4 yuvToRgbConversionMatrix;
  GammaTransferParams gammaDecodeParams;
  GammaTransferParams gammaEncodeParams;
  float3x3 gamutConversionMatrix;
  float3x2 coordTransformationMatrix;
};

Texture2D<float4> ext_tex_plane_1 : register(t2, space0);
cbuffer cbuffer_ext_tex_params : register(b3, space0) {
  uint4 ext_tex_params[13];
};
Texture2D<float4> t : register(t0, space0);
RWTexture2D<float4> outImage : register(u1, space0);

float3 gammaCorrection(float3 v, GammaTransferParams params) {
  const bool3 cond = (abs(v) < float3((params.D).xxx));
  const float3 t_1 = (float3(sign(v)) * ((params.C * abs(v)) + params.F));
  const float3 f = (float3(sign(v)) * (pow(((params.A * abs(v)) + params.B), float3((params.G).xxx)) + params.E));
  return (cond ? t_1 : f);
}

float4 textureLoadExternal(Texture2D<float4> plane0, Texture2D<float4> plane1, int2 coord, ExternalTextureParams params) {
  const int2 coord1 = (coord >> (1u).xx);
  float3 color = float3(0.0f, 0.0f, 0.0f);
  if ((params.numPlanes == 1u)) {
    const uint level_idx = 0u;
    int3 tint_tmp;
    plane0.GetDimensions(0, tint_tmp.x, tint_tmp.y, tint_tmp.z);
    const uint num_levels = tint_tmp.z;
    const uint2 coords = uint2(coord);
    float4 predicated_value = float4(0.0f, 0.0f, 0.0f, 0.0f);
    int3 tint_tmp_1;
    plane0.GetDimensions(min(level_idx, (num_levels - 1u)), tint_tmp_1.x, tint_tmp_1.y, tint_tmp_1.z);
    if (((level_idx < num_levels) & all((coords < tint_tmp_1.xy)))) {
      predicated_value = plane0.Load(uint3(coords, level_idx));
    }
    color = predicated_value.rgb;
  } else {
    const uint level_idx_1 = 0u;
    int3 tint_tmp_2;
    plane0.GetDimensions(0, tint_tmp_2.x, tint_tmp_2.y, tint_tmp_2.z);
    const uint num_levels_1 = tint_tmp_2.z;
    const uint2 coords_1 = uint2(coord);
    float4 predicated_value_1 = float4(0.0f, 0.0f, 0.0f, 0.0f);
    int3 tint_tmp_3;
    plane0.GetDimensions(min(level_idx_1, (num_levels_1 - 1u)), tint_tmp_3.x, tint_tmp_3.y, tint_tmp_3.z);
    if (((level_idx_1 < num_levels_1) & all((coords_1 < tint_tmp_3.xy)))) {
      predicated_value_1 = plane0.Load(uint3(coords_1, level_idx_1));
    }
    const uint level_idx_2 = 0u;
    int3 tint_tmp_4;
    plane1.GetDimensions(0, tint_tmp_4.x, tint_tmp_4.y, tint_tmp_4.z);
    const uint num_levels_2 = tint_tmp_4.z;
    const uint2 coords_2 = uint2(coord1);
    float4 predicated_value_2 = float4(0.0f, 0.0f, 0.0f, 0.0f);
    int3 tint_tmp_5;
    plane1.GetDimensions(min(level_idx_2, (num_levels_2 - 1u)), tint_tmp_5.x, tint_tmp_5.y, tint_tmp_5.z);
    if (((level_idx_2 < num_levels_2) & all((coords_2 < tint_tmp_5.xy)))) {
      predicated_value_2 = plane1.Load(uint3(coords_2, level_idx_2));
    }
    color = mul(params.yuvToRgbConversionMatrix, float4(predicated_value_1.r, predicated_value_2.rg, 1.0f));
  }
  if ((params.doYuvToRgbConversionOnly == 0u)) {
    color = gammaCorrection(color, params.gammaDecodeParams);
    color = mul(color, params.gamutConversionMatrix);
    color = gammaCorrection(color, params.gammaEncodeParams);
  }
  return float4(color, 1.0f);
}

float3x4 ext_tex_params_load_2(uint offset) {
  const uint scalar_offset = ((offset + 0u)) / 4;
  const uint scalar_offset_1 = ((offset + 16u)) / 4;
  const uint scalar_offset_2 = ((offset + 32u)) / 4;
  return float3x4(asfloat(ext_tex_params[scalar_offset / 4]), asfloat(ext_tex_params[scalar_offset_1 / 4]), asfloat(ext_tex_params[scalar_offset_2 / 4]));
}

GammaTransferParams ext_tex_params_load_4(uint offset) {
  const uint scalar_offset_3 = ((offset + 0u)) / 4;
  const uint scalar_offset_4 = ((offset + 4u)) / 4;
  const uint scalar_offset_5 = ((offset + 8u)) / 4;
  const uint scalar_offset_6 = ((offset + 12u)) / 4;
  const uint scalar_offset_7 = ((offset + 16u)) / 4;
  const uint scalar_offset_8 = ((offset + 20u)) / 4;
  const uint scalar_offset_9 = ((offset + 24u)) / 4;
  const uint scalar_offset_10 = ((offset + 28u)) / 4;
  const GammaTransferParams tint_symbol = {asfloat(ext_tex_params[scalar_offset_3 / 4][scalar_offset_3 % 4]), asfloat(ext_tex_params[scalar_offset_4 / 4][scalar_offset_4 % 4]), asfloat(ext_tex_params[scalar_offset_5 / 4][scalar_offset_5 % 4]), asfloat(ext_tex_params[scalar_offset_6 / 4][scalar_offset_6 % 4]), asfloat(ext_tex_params[scalar_offset_7 / 4][scalar_offset_7 % 4]), asfloat(ext_tex_params[scalar_offset_8 / 4][scalar_offset_8 % 4]), asfloat(ext_tex_params[scalar_offset_9 / 4][scalar_offset_9 % 4]), ext_tex_params[scalar_offset_10 / 4][scalar_offset_10 % 4]};
  return tint_symbol;
}

float3x3 ext_tex_params_load_6(uint offset) {
  const uint scalar_offset_11 = ((offset + 0u)) / 4;
  const uint scalar_offset_12 = ((offset + 16u)) / 4;
  const uint scalar_offset_13 = ((offset + 32u)) / 4;
  return float3x3(asfloat(ext_tex_params[scalar_offset_11 / 4].xyz), asfloat(ext_tex_params[scalar_offset_12 / 4].xyz), asfloat(ext_tex_params[scalar_offset_13 / 4].xyz));
}

float3x2 ext_tex_params_load_8(uint offset) {
  const uint scalar_offset_14 = ((offset + 0u)) / 4;
  uint4 ubo_load = ext_tex_params[scalar_offset_14 / 4];
  const uint scalar_offset_15 = ((offset + 8u)) / 4;
  uint4 ubo_load_1 = ext_tex_params[scalar_offset_15 / 4];
  const uint scalar_offset_16 = ((offset + 16u)) / 4;
  uint4 ubo_load_2 = ext_tex_params[scalar_offset_16 / 4];
  return float3x2(asfloat(((scalar_offset_14 & 2) ? ubo_load.zw : ubo_load.xy)), asfloat(((scalar_offset_15 & 2) ? ubo_load_1.zw : ubo_load_1.xy)), asfloat(((scalar_offset_16 & 2) ? ubo_load_2.zw : ubo_load_2.xy)));
}

ExternalTextureParams ext_tex_params_load(uint offset) {
  const uint scalar_offset_17 = ((offset + 0u)) / 4;
  const uint scalar_offset_18 = ((offset + 4u)) / 4;
  const ExternalTextureParams tint_symbol_1 = {ext_tex_params[scalar_offset_17 / 4][scalar_offset_17 % 4], ext_tex_params[scalar_offset_18 / 4][scalar_offset_18 % 4], ext_tex_params_load_2((offset + 16u)), ext_tex_params_load_4((offset + 64u)), ext_tex_params_load_4((offset + 96u)), ext_tex_params_load_6((offset + 128u)), ext_tex_params_load_8((offset + 176u))};
  return tint_symbol_1;
}

[numthreads(1, 1, 1)]
void main() {
  float4 red = textureLoadExternal(t, ext_tex_plane_1, (10).xx, ext_tex_params_load(0u));
  const uint2 coords_3 = (0u).xx;
  int2 tint_tmp_6;
  outImage.GetDimensions(tint_tmp_6.x, tint_tmp_6.y);
  if (all((coords_3 < tint_tmp_6))) {
    outImage[coords_3] = red;
  }
  float4 green = textureLoadExternal(t, ext_tex_plane_1, int2(70, 118), ext_tex_params_load(0u));
  const uint2 coords_4 = uint2(1u, 0u);
  int2 tint_tmp_7;
  outImage.GetDimensions(tint_tmp_7.x, tint_tmp_7.y);
  if (all((coords_4 < tint_tmp_7))) {
    outImage[coords_4] = green;
  }
  return;
}
