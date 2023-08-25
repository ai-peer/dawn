RWByteAddressBuffer prevent_dce : register(u0, space2);

void smoothstep_40864c() {
  float4 arg_0 = float4(2.0f, 2.0f, 2.0f, 2.0f);
  float4 arg_1 = float4(4.0f, 4.0f, 4.0f, 4.0f);
  float4 arg_2 = float4(3.0f, 3.0f, 3.0f, 3.0f);
  float4 res = smoothstep(arg_0, arg_1, arg_2);
  prevent_dce.Store4(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  smoothstep_40864c();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  smoothstep_40864c();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  smoothstep_40864c();
  return;
}
