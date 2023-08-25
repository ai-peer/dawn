Texture3D<float4> arg_0 : register(t0, space1);
SamplerState arg_1 : register(s1, space1);
RWByteAddressBuffer prevent_dce : register(u0, space2);

void textureSampleGrad_21402b() {
  float3 arg_2 = float3(1.0f, 1.0f, 1.0f);
  float3 arg_3 = float3(1.0f, 1.0f, 1.0f);
  float3 arg_4 = float3(1.0f, 1.0f, 1.0f);
  float4 res = arg_0.SampleGrad(arg_1, arg_2, arg_3, arg_4);
  prevent_dce.Store4(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureSampleGrad_21402b();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureSampleGrad_21402b();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureSampleGrad_21402b();
  return;
}
