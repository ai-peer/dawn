[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

uint2 tint_ftou(float2 v) {
  return ((v < float2(4294967040.0f, 4294967040.0f)) ? ((v < float2(0.0f, 0.0f)) ? uint2(0u, 0u) : uint2(v)) : uint2(4294967295u, 4294967295u));
}

static float t = 0.0f;

float2 m() {
  t = 1.0f;
  return float2((t).xx);
}

void f() {
  const float2 tint_symbol = m();
  uint2 v = tint_ftou(tint_symbol);
}
