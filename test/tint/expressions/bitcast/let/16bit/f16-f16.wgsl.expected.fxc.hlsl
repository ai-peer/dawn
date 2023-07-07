SKIP: FAILED

[numthreads(1, 1, 1)]
void f() {
  const float16_t a = float16_t(1.0h);
  const float16_t b = a;
  return;
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x000001DA62937840(3,9-17): error X3000: unrecognized identifier 'float16_t'

