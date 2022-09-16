SKIP: FAILED

[numthreads(1, 1, 1)]
void f() {
  const vector<float16_t, 3> a = vector<float16_t, 3>(float16_t(1.0h), float16_t(2.0h), float16_t(3.0h));
  const float16_t b = float16_t(4.0h);
  const vector<float16_t, 3> r = (a / b);
  return;
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x0000019AEBAF4570(3,16-24): error X3000: syntax error: unexpected token 'float16_t'
D:\Projects\RampUp\dawn\test\tint\Shader@0x0000019AEBAF4570(4,9-17): error X3000: unrecognized identifier 'float16_t'

