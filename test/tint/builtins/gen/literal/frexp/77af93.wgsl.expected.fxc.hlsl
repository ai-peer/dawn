struct frexp_result_vec4_f32 {
  float4 fract;
  int4 exp;
};
void frexp_77af93() {
  frexp_result_vec4_f32 res = {float4(0.5f, 0.5f, 0.5f, 0.5f), int4(1, 1, 1, 1)};
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  frexp_77af93();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  frexp_77af93();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  frexp_77af93();
  return;
}
