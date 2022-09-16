SKIP: FAILED

[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static int t = 0;

int3 m() {
  t = 1;
  return int3((t).xxx);
}

void f() {
  const int3 tint_symbol = m();
  vector<float16_t, 3> v = vector<float16_t, 3>(tint_symbol);
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x0000027639B229D0(15,10-18): error X3000: syntax error: unexpected token 'float16_t'

