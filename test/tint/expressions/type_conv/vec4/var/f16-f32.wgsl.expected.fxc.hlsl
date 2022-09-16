SKIP: FAILED

[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static vector<float16_t, 4> u = (float16_t(1.0h)).xxxx;

void f() {
  const float4 v = float4(u);
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x00000237CA9C2FB0(6,15-23): error X3000: syntax error: unexpected token 'float16_t'

