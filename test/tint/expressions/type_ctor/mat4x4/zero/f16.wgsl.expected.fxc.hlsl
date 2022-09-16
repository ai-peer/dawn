SKIP: FAILED

[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static matrix<float16_t, 4, 4> m = matrix<float16_t, 4, 4>((float16_t(0.0h)).xxxx, (float16_t(0.0h)).xxxx, (float16_t(0.0h)).xxxx, (float16_t(0.0h)).xxxx);
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x0000027540913210(6,15-23): error X3000: syntax error: unexpected token 'float16_t'

