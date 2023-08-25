struct frexp_result_vec2_f32 {
  float2 fract;
  int2 exp;
};
frexp_result_vec2_f32 tint_frexp(float2 param_0) {
  float2 exp;
  float2 fract = sign(param_0) * frexp(param_0, exp);
  frexp_result_vec2_f32 result = {fract, int2(exp)};
  return result;
}

void frexp_eb2421() {
  float2 arg_0 = float2(1.0f, 1.0f);
  frexp_result_vec2_f32 res = tint_frexp(arg_0);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  frexp_eb2421();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  frexp_eb2421();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  frexp_eb2421();
  return;
}
