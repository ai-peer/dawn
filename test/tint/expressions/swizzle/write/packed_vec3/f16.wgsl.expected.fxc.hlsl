SKIP: FAILED

[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

RWByteAddressBuffer U : register(u0, space0);

void f() {
  U.Store<vector<float16_t, 3> >(0u, vector<float16_t, 3>(float16_t(1.0h), float16_t(2.0h), float16_t(3.0h)));
  U.Store<float16_t>(0u, float16_t(1.0h));
  U.Store<float16_t>(2u, float16_t(2.0h));
  U.Store<float16_t>(4u, float16_t(3.0h));
}
FXC validation failure:
D:\Projects\RampUp\dawn\test\tint\Shader@0x000001CF85CC2FF0(9,3-9): error X3018: invalid subscript 'Store'

