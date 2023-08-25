RWByteAddressBuffer prevent_dce : register(u0, space2);

void smoothstep_392c19() {
  float2 arg_0 = float2(2.0f, 2.0f);
  float2 arg_1 = float2(4.0f, 4.0f);
  float2 arg_2 = float2(3.0f, 3.0f);
  float2 res = smoothstep(arg_0, arg_1, arg_2);
  prevent_dce.Store2(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  smoothstep_392c19();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  smoothstep_392c19();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  smoothstep_392c19();
  return;
}
