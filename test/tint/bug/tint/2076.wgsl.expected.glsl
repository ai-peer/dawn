#version 310 es

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
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
  uint pad;
  uint pad_1;
  mat3x4 yuvToRgbConversionMatrix;
  GammaTransferParams gammaDecodeParams;
  GammaTransferParams gammaEncodeParams;
  mat3 gamutConversionMatrix;
  mat3x2 coordTransformationMatrix;
  mat3x2 loadTransformationMatrix;
  vec2 plane0VisibleRectMin;
  vec2 plane0VisibleRectMax;
  vec2 plane1VisibleRectMin;
  vec2 plane1VisibleRectMax;
  uvec2 plane0Size;
  uvec2 plane1Size;
};

struct ExternalTextureParams_std140 {
  uint numPlanes;
  uint doYuvToRgbConversionOnly;
  uint pad;
  uint pad_1;
  mat3x4 yuvToRgbConversionMatrix;
  GammaTransferParams gammaDecodeParams;
  GammaTransferParams gammaEncodeParams;
  mat3 gamutConversionMatrix;
  vec2 coordTransformationMatrix_0;
  vec2 coordTransformationMatrix_1;
  vec2 coordTransformationMatrix_2;
  vec2 loadTransformationMatrix_0;
  vec2 loadTransformationMatrix_1;
  vec2 loadTransformationMatrix_2;
  vec2 plane0VisibleRectMin;
  vec2 plane0VisibleRectMax;
  vec2 plane1VisibleRectMin;
  vec2 plane1VisibleRectMax;
  uvec2 plane0Size;
  uvec2 plane1Size;
};

layout(binding = 4, std140) uniform ext_tex_params_block_std140_ubo {
  ExternalTextureParams_std140 inner;
} ext_tex_params;

