void round_a1673d() {
  float3 res = float3(4.0f, 4.0f, 4.0f);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  round_a1673d();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  round_a1673d();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  round_a1673d();
  return;
}
