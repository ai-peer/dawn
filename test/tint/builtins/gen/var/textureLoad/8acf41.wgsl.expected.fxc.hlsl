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
};

Texture2D<float4> ext_tex_plane_1 : register(t1, space1);
cbuffer cbuffer_ext_tex_params : register(b2, space1) {
  uint4 ext_tex_params[11];
};
Texture2D<float4> arg_0 : register(t0, space1);

float3 gammaCorrection(float3 v, GammaTransferParams params) {
  const bool3 cond = (abs(v) < float3((params.D).xxx));
  const float3 t = (sign(v) * ((params.C * abs(v)) + params.F));
  const float3 f = (sign(v) * (pow(((params.A * abs(v)) + params.B), float3((params.G).xxx)) + params.E));
  return (cond ? t : f);
}

float4 textureLoadExternal(Texture2D<float4> plane0, Texture2D<float4> plane1, int2 coord, ExternalTextureParams params) {
  float3 color = float3(0.0f, 0.0f, 0.0f);
  if ((params.numPlanes == 1u)) {
    color = plane0.Load(int3(coord, 0)).rgb;
  } else {
    color = mul(params.yuvToRgbConversionMatrix, float4(plane0.Load(int3(coord, 0)).r, plane1.Load(int3(coord, 0)).rg, 1.0f));
  }
  if ((params.doYuvToRgbConversionOnly == 0u)) {
    color = gammaCorrection(color, params.gammaDecodeParams);
    color = mul(color, params.gamutConversionMatrix);
    color = gammaCorrection(color, params.gammaEncodeParams);
  }
  return float4(color, 1.0f);
}

float3x4 tint_symbol_3(uint4 buffer[11], uint offset) {
  const uint scalar_offset_bytes = ((offset + 0u));
  const uint scalar_offset_index = scalar_offset_bytes / 4;
  const uint scalar_offset_bytes_1 = ((offset + 16u));
  const uint scalar_offset_index_1 = scalar_offset_bytes_1 / 4;
  const uint scalar_offset_bytes_2 = ((offset + 32u));
  const uint scalar_offset_index_2 = scalar_offset_bytes_2 / 4;
  return float3x4(asfloat(buffer[scalar_offset_index / 4]), asfloat(buffer[scalar_offset_index_1 / 4]), asfloat(buffer[scalar_offset_index_2 / 4]));
}

GammaTransferParams tint_symbol_5(uint4 buffer[11], uint offset) {
  const uint scalar_offset_bytes_3 = ((offset + 0u));
  const uint scalar_offset_index_3 = scalar_offset_bytes_3 / 4;
  const uint scalar_offset_bytes_4 = ((offset + 4u));
  const uint scalar_offset_index_4 = scalar_offset_bytes_4 / 4;
  const uint scalar_offset_bytes_5 = ((offset + 8u));
  const uint scalar_offset_index_5 = scalar_offset_bytes_5 / 4;
  const uint scalar_offset_bytes_6 = ((offset + 12u));
  const uint scalar_offset_index_6 = scalar_offset_bytes_6 / 4;
  const uint scalar_offset_bytes_7 = ((offset + 16u));
  const uint scalar_offset_index_7 = scalar_offset_bytes_7 / 4;
  const uint scalar_offset_bytes_8 = ((offset + 20u));
  const uint scalar_offset_index_8 = scalar_offset_bytes_8 / 4;
  const uint scalar_offset_bytes_9 = ((offset + 24u));
  const uint scalar_offset_index_9 = scalar_offset_bytes_9 / 4;
  const uint scalar_offset_bytes_10 = ((offset + 28u));
  const uint scalar_offset_index_10 = scalar_offset_bytes_10 / 4;
  const GammaTransferParams tint_symbol_9 = {asfloat(buffer[scalar_offset_index_3 / 4][scalar_offset_index_3 % 4]), asfloat(buffer[scalar_offset_index_4 / 4][scalar_offset_index_4 % 4]), asfloat(buffer[scalar_offset_index_5 / 4][scalar_offset_index_5 % 4]), asfloat(buffer[scalar_offset_index_6 / 4][scalar_offset_index_6 % 4]), asfloat(buffer[scalar_offset_index_7 / 4][scalar_offset_index_7 % 4]), asfloat(buffer[scalar_offset_index_8 / 4][scalar_offset_index_8 % 4]), asfloat(buffer[scalar_offset_index_9 / 4][scalar_offset_index_9 % 4]), buffer[scalar_offset_index_10 / 4][scalar_offset_index_10 % 4]};
  return tint_symbol_9;
}

float3x3 tint_symbol_7(uint4 buffer[11], uint offset) {
  const uint scalar_offset_bytes_11 = ((offset + 0u));
  const uint scalar_offset_index_11 = scalar_offset_bytes_11 / 4;
  const uint scalar_offset_bytes_12 = ((offset + 16u));
  const uint scalar_offset_index_12 = scalar_offset_bytes_12 / 4;
  const uint scalar_offset_bytes_13 = ((offset + 32u));
  const uint scalar_offset_index_13 = scalar_offset_bytes_13 / 4;
  return float3x3(asfloat(buffer[scalar_offset_index_11 / 4].xyz), asfloat(buffer[scalar_offset_index_12 / 4].xyz), asfloat(buffer[scalar_offset_index_13 / 4].xyz));
}

ExternalTextureParams tint_symbol_1(uint4 buffer[11], uint offset) {
  const uint scalar_offset_bytes_14 = ((offset + 0u));
  const uint scalar_offset_index_14 = scalar_offset_bytes_14 / 4;
  const uint scalar_offset_bytes_15 = ((offset + 4u));
  const uint scalar_offset_index_15 = scalar_offset_bytes_15 / 4;
  const ExternalTextureParams tint_symbol_10 = {buffer[scalar_offset_index_14 / 4][scalar_offset_index_14 % 4], buffer[scalar_offset_index_15 / 4][scalar_offset_index_15 % 4], tint_symbol_3(buffer, (offset + 16u)), tint_symbol_5(buffer, (offset + 64u)), tint_symbol_5(buffer, (offset + 96u)), tint_symbol_7(buffer, (offset + 128u))};
  return tint_symbol_10;
}

void textureLoad_8acf41() {
  int2 arg_1 = (0).xx;
  float4 res = textureLoadExternal(arg_0, ext_tex_plane_1, arg_1, tint_symbol_1(ext_tex_params, 0u));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureLoad_8acf41();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureLoad_8acf41();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureLoad_8acf41();
  return;
}
