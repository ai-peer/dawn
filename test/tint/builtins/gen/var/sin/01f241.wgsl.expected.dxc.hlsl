RWByteAddressBuffer prevent_dce : register(u0, space2);

void sin_01f241() {
  float3 arg_0 = float3(1.57079637050628662109f, 1.57079637050628662109f, 1.57079637050628662109f);
  float3 res = sin(arg_0);
  prevent_dce.Store3(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  sin_01f241();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  sin_01f241();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  sin_01f241();
  return;
}
