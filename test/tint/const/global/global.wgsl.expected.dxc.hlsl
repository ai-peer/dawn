struct tint_symbol {
  float4 value : SV_Target0;
};

float4 main_inner() {
  int v1 = 1;
  uint v2 = 1u;
  float v3 = 1.0f;
  int3 v4 = int3(1, 1, 1);
  uint3 v5 = uint3(1u, 1u, 1u);
  float3 v6 = float3(1.0f, 1.0f, 1.0f);
  float3x3 v7 = float3x3(float3(1.0f, 1.0f, 1.0f), float3(1.0f, 1.0f, 1.0f), float3(1.0f, 1.0f, 1.0f));
  float v9[10] = (float[10])0;
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol main() {
  const float4 inner_result = main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}
