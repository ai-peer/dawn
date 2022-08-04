SKIP: FAILED

struct frexp_result_vec4_f16 {
  vector<float16_t, 4> sig;
  int4 exp;
};
frexp_result_vec4_f16 tint_frexp(vector<float16_t, 4> param_0) {
  vector<float16_t, 4> exp;
  vector<float16_t, 4> sig = frexp(param_0, exp);
  frexp_result_vec4_f16 result = {sig, int4(exp)};
  return result;
}

void frexp_77dfa6() {
  vector<float16_t, 4> arg_0 = (float16_t(0.0h)).xxxx;
  frexp_result_vec4_f16 res = tint_frexp(arg_0);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  frexp_77dfa6();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  frexp_77dfa6();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  frexp_77dfa6();
  return;
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\builtins\Shader@0x000001A82DC886C0(2,10-18): error X3000: syntax error: unexpected token 'float16_t'

