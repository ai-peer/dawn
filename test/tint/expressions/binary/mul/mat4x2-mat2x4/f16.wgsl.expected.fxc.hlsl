SKIP: FAILED

[numthreads(1, 1, 1)]
void f() {
  const matrix<float16_t, 4, 2> a = matrix<float16_t, 4, 2>(vector<float16_t, 2>(float16_t(-1.0h), float16_t(-2.0h)), vector<float16_t, 2>(float16_t(-3.0h), float16_t(-4.0h)), vector<float16_t, 2>(float16_t(-5.0h), float16_t(-6.0h)), vector<float16_t, 2>(float16_t(-7.0h), float16_t(-8.0h)));
  const matrix<float16_t, 2, 4> b = matrix<float16_t, 2, 4>(vector<float16_t, 4>(float16_t(1.0h), float16_t(2.0h), float16_t(3.0h), float16_t(4.0h)), vector<float16_t, 4>(float16_t(5.0h), float16_t(6.0h), float16_t(7.0h), float16_t(8.0h)));
  const matrix<float16_t, 2, 2> r = mul(b, a);
  return;
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x000001D15A2A3540(3,16-24): error X3000: syntax error: unexpected token 'float16_t'
D:\Projects\RampUp\dawn\test\tint\Shader@0x000001D15A2A3540(4,16-24): error X3000: syntax error: unexpected token 'float16_t'
D:\Projects\RampUp\dawn\test\tint\Shader@0x000001D15A2A3540(5,16-24): error X3000: syntax error: unexpected token 'float16_t'

