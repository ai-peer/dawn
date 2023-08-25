RWByteAddressBuffer prevent_dce : register(u0, space2);

void degrees_2af623() {
  float3 res = float3(57.2957763671875f, 57.2957763671875f, 57.2957763671875f);
  prevent_dce.Store3(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  degrees_2af623();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  degrees_2af623();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  degrees_2af623();
  return;
}
