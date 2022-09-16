SKIP: FAILED

[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static float t = 0.0f;

float4 m() {
  t = 1.0f;
  return float4((t).xxxx);
}

void f() {
  const float4 tint_symbol = m();
  vector<float16_t, 4> v = vector<float16_t, 4>(tint_symbol);
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x000001E4ED4D1F50(15,10-18): error X3000: syntax error: unexpected token 'float16_t'

