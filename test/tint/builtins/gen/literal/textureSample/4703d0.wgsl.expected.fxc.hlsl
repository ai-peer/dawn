Texture2DArray arg_0 : register(t0, space1);
SamplerState arg_1 : register(s1, space1);
RWByteAddressBuffer prevent_dce : register(u0, space2);

void textureSample_4703d0() {
  float res = arg_0.Sample(arg_1, float3(float2(1.0f, 1.0f), float(1u)), int2(1, 1)).x;
  prevent_dce.Store(0u, asuint(res));
}

void fragment_main() {
  textureSample_4703d0();
  return;
}
