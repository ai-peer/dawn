SKIP: FAILED

[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static vector<float16_t, 2> v = vector<float16_t, 2>(float16_t(0.0h), float16_t(1.0h));
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x000001EE6A632580(6,15-23): error X3000: syntax error: unexpected token 'float16_t'

