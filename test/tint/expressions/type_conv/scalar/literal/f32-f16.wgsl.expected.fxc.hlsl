SKIP: FAILED

[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static float16_t u = float16_t(1.0h);
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x0000026C36A03070(6,8-16): error X3000: unrecognized identifier 'float16_t'

