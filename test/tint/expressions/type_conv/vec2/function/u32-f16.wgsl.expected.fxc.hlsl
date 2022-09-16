SKIP: FAILED

[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static uint t = 0u;

uint2 m() {
  t = 1u;
  return uint2((t).xx);
}

void f() {
  const uint2 tint_symbol = m();
  vector<float16_t, 2> v = vector<float16_t, 2>(tint_symbol);
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x000001DE01754810(15,10-18): error X3000: syntax error: unexpected token 'float16_t'

