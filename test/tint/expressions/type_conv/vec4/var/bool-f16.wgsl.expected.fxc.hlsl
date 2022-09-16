SKIP: FAILED

[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static bool4 u = (true).xxxx;

void f() {
  const vector<float16_t, 4> v = vector<float16_t, 4>(u);
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x00000248A9812E80(9,16-24): error X3000: syntax error: unexpected token 'float16_t'

