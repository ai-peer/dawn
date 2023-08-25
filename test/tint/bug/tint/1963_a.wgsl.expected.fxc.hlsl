[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void X(float2 a, float2 b) {
}

float2 Y() {
  return float2(0.0f, 0.0f);
}

void f() {
  float2 v = float2(0.0f, 0.0f);
  X(float2(0.0f, 0.0f), v);
  const float2 tint_symbol = float2(0.0f, 0.0f);
  const float2 tint_symbol_1 = Y();
  X(tint_symbol, tint_symbol_1);
}
