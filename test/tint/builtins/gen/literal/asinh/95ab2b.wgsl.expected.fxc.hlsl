SKIP: FAILED

void asinh_95ab2b() {
  vector<float16_t, 4> res = (float16_t(0.0h)).xxxx;
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  asinh_95ab2b();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  asinh_95ab2b();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  asinh_95ab2b();
  return;
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x0000018223B7C0D0(2,10-18): error X3000: syntax error: unexpected token 'float16_t'

