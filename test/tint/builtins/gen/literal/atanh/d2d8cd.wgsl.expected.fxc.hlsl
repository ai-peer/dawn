SKIP: FAILED

void atanh_d2d8cd() {
  float16_t res = float16_t(0.0h);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  atanh_d2d8cd();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  atanh_d2d8cd();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  atanh_d2d8cd();
  return;
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x0000027051FC1620(2,3-11): error X3000: unrecognized identifier 'float16_t'
D:\Projects\RampUp\dawn\test\tint\Shader@0x0000027051FC1620(2,13-15): error X3000: unrecognized identifier 'res'

