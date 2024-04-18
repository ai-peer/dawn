int2 tint_ftoi(float2 v) {
  return ((v < (2147483520.0f).xx) ? ((v < (-2147483648.0f).xx) ? (-2147483648).xx : int2(v)) : (2147483647).xx);
}

int2 tint_clamp(int2 e, int2 low, int2 high) {
  return min(max(e, low), high);
}

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
  float2 visibleRectMin;
  float2 visibleRectMax;
  uint2 plane0Size;
  uint2 plane1Size;
  uint2 displayVisibleSize;
};

Texture2D<float4> ext_tex_plane_1 : register(t2);
cbuffer cbuffer_ext_tex_params : register(b3) {
  uint4 ext_tex_params[15];
};
Texture2D<float4> t : register(t0);
RWTexture2D<float4> outImage : register(u1);

float3 gammaCorrection(float3 v, GammaTransferParams params) {
  bool3 cond = (abs(v) < float3((params.D).xxx));
  float3 t = (float3(sign(v)) * ((params.C * abs(v)) + params.F));
  float3 f = (float3(sign(v)) * (pow(((params.A * abs(v)) + params.B), float3((params.G).xxx)) + params.E));
  return (cond ? t : f);
}

float4 textureLoadExternal(Texture2D<float4> plane0, Texture2D<float4> plane1, int2 coord, ExternalTextureParams params) {
  float3x3 toTexel = float3x3(float3(float(params.plane0Size.x), 0.0f, 0.0f), float3(0.0f, float(params.plane0Size.y), 0.0f), float3(0.0f, 0.0f, 1.0f));
  float3x3 toNormalize = float3x3(float3((1.0f / float(params.displayVisibleSize.x)), 0.0f, 0.0f), float3(0.0f, (1.0f / float(params.displayVisibleSize.y)), 0.0f), float3(0.0f, 0.0f, 1.0f));
  float3x3 loadTransformationMatrix = mul(mul(float3x3(float3(params.coordTransformationMatrix[0], 0.0f), float3(params.coordTransformationMatrix[1], 0.0f), float3(params.coordTransformationMatrix[2], 1.0f)), toTexel), toNormalize);
  float3 modifiedCoords = mul(float3(float2(coord), 1.0f), loadTransformationMatrix);
  int2 plane0_clamped = tint_clamp(tint_ftoi(modifiedCoords.xy), tint_ftoi((params.visibleRectMin * float2(params.plane0Size))), (tint_ftoi((params.visibleRectMax * float2(params.plane0Size))) - (1).xx));
  float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);
  if ((params.numPlanes == 1u)) {
    color = plane0.Load(int3(plane0_clamped, 0)).rgba;
  } else {
    int2 coord1 = tint_ftoi((float2(plane0_clamped) * (float2(params.plane1Size) / float2(params.plane0Size))));
    int2 plane1_clamped = tint_clamp(int2(coord1), tint_ftoi((params.visibleRectMin * float2(params.plane1Size))), tint_ftoi((params.visibleRectMax * float2(params.plane1Size))));
    color = float4(mul(params.yuvToRgbConversionMatrix, float4(plane0.Load(int3(plane0_clamped, 0)).r, plane1.Load(int3(plane1_clamped, 0)).rg, 1.0f)), 1.0f);
  }
  if ((params.doYuvToRgbConversionOnly == 0u)) {
    color = float4(gammaCorrection(color.rgb, params.gammaDecodeParams), color.a);
    color = float4(mul(color.rgb, params.gamutConversionMatrix), color.a);
    color = float4(gammaCorrection(color.rgb, params.gammaEncodeParams), color.a);
  }
  return color;
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
  GammaTransferParams tint_symbol = {asfloat(ext_tex_params[scalar_offset_3 / 4][scalar_offset_3 % 4]), asfloat(ext_tex_params[scalar_offset_4 / 4][scalar_offset_4 % 4]), asfloat(ext_tex_params[scalar_offset_5 / 4][scalar_offset_5 % 4]), asfloat(ext_tex_params[scalar_offset_6 / 4][scalar_offset_6 % 4]), asfloat(ext_tex_params[scalar_offset_7 / 4][scalar_offset_7 % 4]), asfloat(ext_tex_params[scalar_offset_8 / 4][scalar_offset_8 % 4]), asfloat(ext_tex_params[scalar_offset_9 / 4][scalar_offset_9 % 4]), ext_tex_params[scalar_offset_10 / 4][scalar_offset_10 % 4]};
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
  const uint scalar_offset_19 = ((offset + 200u)) / 4;
  uint4 ubo_load_3 = ext_tex_params[scalar_offset_19 / 4];
  const uint scalar_offset_20 = ((offset + 208u)) / 4;
  uint4 ubo_load_4 = ext_tex_params[scalar_offset_20 / 4];
  const uint scalar_offset_21 = ((offset + 216u)) / 4;
  uint4 ubo_load_5 = ext_tex_params[scalar_offset_21 / 4];
  const uint scalar_offset_22 = ((offset + 224u)) / 4;
  uint4 ubo_load_6 = ext_tex_params[scalar_offset_22 / 4];
  const uint scalar_offset_23 = ((offset + 232u)) / 4;
  uint4 ubo_load_7 = ext_tex_params[scalar_offset_23 / 4];
  ExternalTextureParams tint_symbol_1 = {ext_tex_params[scalar_offset_17 / 4][scalar_offset_17 % 4], ext_tex_params[scalar_offset_18 / 4][scalar_offset_18 % 4], ext_tex_params_load_2((offset + 16u)), ext_tex_params_load_4((offset + 64u)), ext_tex_params_load_4((offset + 96u)), ext_tex_params_load_6((offset + 128u)), ext_tex_params_load_8((offset + 176u)), asfloat(((scalar_offset_19 & 2) ? ubo_load_3.zw : ubo_load_3.xy)), asfloat(((scalar_offset_20 & 2) ? ubo_load_4.zw : ubo_load_4.xy)), ((scalar_offset_21 & 2) ? ubo_load_5.zw : ubo_load_5.xy), ((scalar_offset_22 & 2) ? ubo_load_6.zw : ubo_load_6.xy), ((scalar_offset_23 & 2) ? ubo_load_7.zw : ubo_load_7.xy)};
  return tint_symbol_1;
}

[numthreads(1, 1, 1)]
void main() {
  float4 red = textureLoadExternal(t, ext_tex_plane_1, (10).xx, ext_tex_params_load(0u));
  outImage[(0).xx] = red;
  float4 green = textureLoadExternal(t, ext_tex_plane_1, int2(70, 118), ext_tex_params_load(0u));
  outImage[int2(1, 0)] = green;
  return;
}
