[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

int3 tint_ftoi(float3 v) {
  return ((v < float3(2147483520.0f, 2147483520.0f, 2147483520.0f)) ? ((v < float3(-2147483648.0f, -2147483648.0f, -2147483648.0f)) ? int3(-2147483648, -2147483648, -2147483648) : int3(v)) : int3(2147483647, 2147483647, 2147483647));
}

static float t = 0.0f;

float3 m() {
  t = 1.0f;
  return float3((t).xxx);
}

void f() {
  const float3 tint_symbol = m();
  int3 v = tint_ftoi(tint_symbol);
}
