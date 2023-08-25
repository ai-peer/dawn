Texture2DArray arg_0 : register(t0, space1);
SamplerState arg_1 : register(s1, space1);
RWByteAddressBuffer prevent_dce : register(u0, space2);

void textureSample_1a4e1b() {
  float res = arg_0.Sample(arg_1, float3(float2(1.0f, 1.0f), float(1u))).x;
  prevent_dce.Store(0u, asuint(res));
}

void fragment_main() {
  textureSample_1a4e1b();
  return;
}
