RWTexture2D<float4> t_rgba8unorm : register(u0);
RWTexture2D<float4> t_rgba8snorm : register(u1);
RWTexture2D<uint4> t_rgba8uint : register(u2);
RWTexture2D<int4> t_rgba8sint : register(u3);
RWTexture2D<uint4> t_rgba16uint : register(u4);
RWTexture2D<int4> t_rgba16sint : register(u5);
RWTexture2D<float4> t_rgba16float : register(u6);
RWTexture2D<uint4> t_r32uint : register(u7);
RWTexture2D<int4> t_r32sint : register(u8);
RWTexture2D<float4> t_r32float : register(u9);
RWTexture2D<uint4> t_rg32uint : register(u10);
RWTexture2D<int4> t_rg32sint : register(u11);
RWTexture2D<float4> t_rg32float : register(u12);
RWTexture2D<uint4> t_rgba32uint : register(u13);
RWTexture2D<int4> t_rgba32sint : register(u14);
RWTexture2D<float4> t_rgba32float : register(u15);

[numthreads(1, 1, 1)]
void main() {
  uint2 tint_tmp;
  t_rgba8unorm.GetDimensions(tint_tmp.x, tint_tmp.y);
  uint2 dim1 = tint_tmp;
  uint2 tint_tmp_1;
  t_rgba8snorm.GetDimensions(tint_tmp_1.x, tint_tmp_1.y);
  uint2 dim2 = tint_tmp_1;
  uint2 tint_tmp_2;
  t_rgba8uint.GetDimensions(tint_tmp_2.x, tint_tmp_2.y);
  uint2 dim3 = tint_tmp_2;
  uint2 tint_tmp_3;
  t_rgba8sint.GetDimensions(tint_tmp_3.x, tint_tmp_3.y);
  uint2 dim4 = tint_tmp_3;
  uint2 tint_tmp_4;
  t_rgba16uint.GetDimensions(tint_tmp_4.x, tint_tmp_4.y);
  uint2 dim5 = tint_tmp_4;
  uint2 tint_tmp_5;
  t_rgba16sint.GetDimensions(tint_tmp_5.x, tint_tmp_5.y);
  uint2 dim6 = tint_tmp_5;
  uint2 tint_tmp_6;
  t_rgba16float.GetDimensions(tint_tmp_6.x, tint_tmp_6.y);
  uint2 dim7 = tint_tmp_6;
  uint2 tint_tmp_7;
  t_r32uint.GetDimensions(tint_tmp_7.x, tint_tmp_7.y);
  uint2 dim8 = tint_tmp_7;
  uint2 tint_tmp_8;
  t_r32sint.GetDimensions(tint_tmp_8.x, tint_tmp_8.y);
  uint2 dim9 = tint_tmp_8;
  uint2 tint_tmp_9;
  t_r32float.GetDimensions(tint_tmp_9.x, tint_tmp_9.y);
  uint2 dim10 = tint_tmp_9;
  uint2 tint_tmp_10;
  t_rg32uint.GetDimensions(tint_tmp_10.x, tint_tmp_10.y);
  uint2 dim11 = tint_tmp_10;
  uint2 tint_tmp_11;
  t_rg32sint.GetDimensions(tint_tmp_11.x, tint_tmp_11.y);
  uint2 dim12 = tint_tmp_11;
  uint2 tint_tmp_12;
  t_rg32float.GetDimensions(tint_tmp_12.x, tint_tmp_12.y);
  uint2 dim13 = tint_tmp_12;
  uint2 tint_tmp_13;
  t_rgba32uint.GetDimensions(tint_tmp_13.x, tint_tmp_13.y);
  uint2 dim14 = tint_tmp_13;
  uint2 tint_tmp_14;
  t_rgba32sint.GetDimensions(tint_tmp_14.x, tint_tmp_14.y);
  uint2 dim15 = tint_tmp_14;
  uint2 tint_tmp_15;
  t_rgba32float.GetDimensions(tint_tmp_15.x, tint_tmp_15.y);
  uint2 dim16 = tint_tmp_15;
  return;
}