RWByteAddressBuffer prevent_dce : register(u0, space2);

void step_e2b337() {
  float4 arg_0 = float4(1.0f, 1.0f, 1.0f, 1.0f);
  float4 arg_1 = float4(1.0f, 1.0f, 1.0f, 1.0f);
  float4 res = step(arg_0, arg_1);
  prevent_dce.Store4(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  step_e2b337();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  step_e2b337();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  step_e2b337();
  return;
}
