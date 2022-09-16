SKIP: FAILED

[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static float16_t u = float16_t(1.0h);

void f() {
  const bool v = bool(u);
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x000001B8D62D2620(6,8-16): error X3000: unrecognized identifier 'float16_t'

