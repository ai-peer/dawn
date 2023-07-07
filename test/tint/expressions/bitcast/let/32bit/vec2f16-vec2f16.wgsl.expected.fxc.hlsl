SKIP: FAILED

[numthreads(1, 1, 1)]
void f() {
  const vector<float16_t, 2> a = vector<float16_t, 2>(float16_t(1.0h), float16_t(2.0h));
  const vector<float16_t, 2> b = a;
  return;
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x000002213FC3FC80(3,16-24): error X3000: syntax error: unexpected token 'float16_t'
D:\Projects\RampUp\dawn\test\tint\Shader@0x000002213FC3FC80(4,16-24): error X3000: syntax error: unexpected token 'float16_t'

