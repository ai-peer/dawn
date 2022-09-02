void tint_symbol() {
  const int x = 2;
}

void tint_symbol_1() {
  const float2 x = float2(3.0f, 4.0f);
}

void fixed_size_array() {
  const int arr[2] = {1, 2};
  const int x = arr[3];
}

ByteAddressBuffer rarr : register(t0, space0);

void runtime_size_array() {
  const float x = asfloat(rarr.Load(0u));
}

[numthreads(1, 1, 1)]
void f() {
  tint_symbol();
  tint_symbol_1();
  fixed_size_array();
  runtime_size_array();
  return;
}
